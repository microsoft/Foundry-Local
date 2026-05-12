// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Runtime.CompilerServices;
using System.Threading.Channels;

using Betalgo.Ranul.OpenAI.ObjectModels.RealtimeModels;
using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail.Interop;

using Api = Microsoft.AI.Foundry.Local.Detail.Native.Api;
using NativeModel = Microsoft.AI.Foundry.Local.Detail.Native.Model;
using NativeSession = Microsoft.AI.Foundry.Local.Detail.Native.Session;

#pragma warning disable IDISP001 // Dispose created — ownership transfers to Request/Queue
#pragma warning disable IDISP003 // Dispose previous before re-assigning — fields assigned once in StartAsync
#pragma warning disable CA2000   // Dispose objects before losing scope — ownership transfers

/// <summary>
/// Session for real-time audio streaming ASR (Automatic Speech Recognition).
/// Push PCM audio chunks via <see cref="AppendAsync"/> and consume transcription results
/// via <see cref="GetStream"/>.
/// </summary>
public sealed class LiveAudioTranscriptionSession : IAsyncDisposable
{
    private enum SessionState { Created, Started, Stopped, Disposed }

    private readonly string _modelId;
    private readonly NativeModel _nativeModel;

    private SessionState _state = SessionState.Created;
    private ItemQueue? _queue;
    private NativeSession? _session;
    private Request? _request;
    private Channel<LiveAudioTranscriptionResponse>? _channel;
    private Task? _processingTask;
    private CancellationTokenSource? _stopCts;

    /// <summary>
    /// Audio format settings for the streaming session.
    /// </summary>
    public record LiveAudioTranscriptionOptions
    {
        public int SampleRate { get; set; } = 16000;
        public int Channels { get; set; } = 1;
        public int BitsPerSample { get; set; } = 16;
        public string? Language { get; set; }
        public int PushQueueCapacity { get; set; } = 100;
    }

    public LiveAudioTranscriptionOptions Settings { get; } = new();

    internal LiveAudioTranscriptionSession(string modelId, NativeModel nativeModel)
    {
        _modelId = modelId;
        _nativeModel = nativeModel;
    }

    public Task StartAsync(CancellationToken ct = default)
    {
        if (_state != SessionState.Created)
        {
            throw new FoundryLocalException("Session can only be started once and must be in Created state.");
        }

        var formatDescriptor = AudioItem.CreateFormatDescriptor("pcm", Settings.SampleRate, Settings.Channels);

        _queue = new ItemQueue();

        _channel = Channel.CreateUnbounded<LiveAudioTranscriptionResponse>(
            new UnboundedChannelOptions
            {
                SingleWriter = true,
                SingleReader = true,
                AllowSynchronousContinuations = true
            });

        _session = new NativeSession(_nativeModel);

        var channel = _channel;

        _stopCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        var stopToken = _stopCts.Token;

        FlStreamingCallback streamingCallback = (FlStreamingCallbackData data, IntPtr userData) =>
        {
            try
            {
                if (data.ItemQueue != IntPtr.Zero)
                {
                    while (Api.Item.QueueTryPop(data.ItemQueue, out var itemPtr))
                    {
                        using var item = Item.FromNative(itemPtr, ownsHandle: true);

                        LiveAudioTranscriptionResponse? response = null;

                        if (item is TextItem textItem && !string.IsNullOrEmpty(textItem.Text))
                        {
                            if (textItem.Type == TextItemType.OpenAIJson)
                            {
                                // JSON path — structured AudioTranscriptionResponse from web/JSON input
                                response = LiveAudioTranscriptionResponse.FromJson(textItem.Text);
                            }
                            else
                            {
                                // Direct streaming path — raw text tokens from AudioSession
                                response = new LiveAudioTranscriptionResponse
                                {
                                    IsFinal = true,
                                    Content =
                                    [
                                        new ContentPart
                                        {
                                            Text = textItem.Text,
                                            Transcript = textItem.Text
                                        }
                                    ]
                                };
                            }
                        }

                        if (response != null)
                        {
                            channel.Writer.TryWrite(response);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error processing live audio transcription callback data.", ex));
            }

            return stopToken.IsCancellationRequested ? 1 : 0;
        };

        _session.SetStreamingCallback(streamingCallback);

        _request = new Request();
        _request.AddItem(formatDescriptor); // transfers ownership

        // Add queue without taking ownership — we still need to push items into it
        Api.CheckStatus(Api.Inference.RequestAddItem(_request.Ptr, _queue.Ptr, false));

        _processingTask = Task.Run(() =>
        {
            try
            {
                var responsePtr = _session.ProcessRequest(_request.Ptr);
                Api.Inference.ResponseRelease(responsePtr);
                channel.Writer.TryComplete();
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error during live audio transcription processing.", ex));
            }
            catch (OperationCanceledException)
            {
                channel.Writer.TryComplete();
            }
        }, ct);

        _state = SessionState.Started;
        return Task.CompletedTask;
    }

    public ValueTask AppendAsync(ReadOnlyMemory<byte> pcmData, CancellationToken ct = default)
    {
        if (_state != SessionState.Started)
        {
            throw new FoundryLocalException("Session has not been started. Call StartAsync first.");
        }

        var bytesItem = BytesItem.CreateOwned(pcmData);
        _queue!.Push(bytesItem); // transfers ownership

        return ValueTask.CompletedTask;
    }

    public async IAsyncEnumerable<LiveAudioTranscriptionResponse> GetStream(
        [EnumeratorCancellation] CancellationToken ct = default)
    {
        if (_state != SessionState.Started)
        {
            throw new FoundryLocalException("Session has not been started. Call StartAsync first.");
        }

        await foreach (var item in _channel!.Reader.ReadAllAsync(ct).ConfigureAwait(false))
        {
            yield return item;
        }
    }

    public async Task StopAsync(CancellationToken ct = default)
    {
        if (_state != SessionState.Started)
        {
            throw new FoundryLocalException("Session must be in Started state to stop.");
        }

        _queue!.MarkFinished();

        try { _stopCts?.Cancel(); } catch { }

        if (_processingTask != null)
        {
            await _processingTask.ConfigureAwait(false);
        }

        _state = SessionState.Stopped;
    }

    public async ValueTask DisposeAsync()
    {
        if (_state == SessionState.Disposed)
        {
            return;
        }

        if (_state == SessionState.Started)
        {
            _queue?.MarkFinished();

            try { _stopCts?.Cancel(); } catch { }

            if (_processingTask != null)
            {
                try
                {
                    await _processingTask.ConfigureAwait(false);
                }
                catch
                {
                    // Best-effort cleanup during dispose
                }
            }
        }

        _queue?.Dispose();
        _session?.Dispose();
        _request?.Dispose();
        _stopCts?.Dispose();

        _state = SessionState.Disposed;
    }
}
