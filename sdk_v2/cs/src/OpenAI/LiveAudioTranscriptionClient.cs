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
        Detail.Throw.IfDisposed(_state == SessionState.Disposed, this);

        if (_state != SessionState.Created)
        {
            throw new FoundryLocalException($"Session can only be started once (was {_state}).");
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
            bool errored = false;

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
                            // using the direct API only so should be just the text currently
                            // TODO: Do we need the other custom fields we added and a new item type?
                            System.Diagnostics.Debug.Assert(textItem.Type != TextItemType.OpenAIJson,
                                "Unexpected TextItem type in streaming callback: " + textItem.Type);

                            // Direct streaming path — raw text tokens from AudioSession.
                            // Matches legacy SDK semantics which doesn't conform to either 
                            // OAI transcription streaming or OAI realtime API types/semantics.
                            response = new LiveAudioTranscriptionResponse
                            {
                                IsFinal = false,
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

                        if (response != null)
                        {
                            channel.Writer.TryWrite(response);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                errored = true;
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error processing live audio transcription callback data.", ex));
            }

            return errored || stopToken.IsCancellationRequested ? 1 : 0;
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

                // Drain the final Response: it carries the aggregated transcription as TextItem(s)
                using (var response = new Response(responsePtr))
                {
                    var finalText = new System.Text.StringBuilder();
                    foreach (var responseItem in response)
                    {
                        using (responseItem)
                        {
                            if (responseItem is TextItem finalTextItem && !string.IsNullOrEmpty(finalTextItem.Text))
                            {
                                finalText.Append(finalTextItem.Text);
                            }
                        }
                    }

                    if (finalText.Length > 0)
                    {
                        channel.Writer.TryWrite(new LiveAudioTranscriptionResponse
                        {
                            IsFinal = true,
                            Content =
                            [
                                new ContentPart { Text = finalText.ToString() }
                            ]
                        });
                    }
                }

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
        Detail.Throw.IfDisposed(_state == SessionState.Disposed, this);

        if (_state != SessionState.Started)
        {
            throw new FoundryLocalException($"Session must be Started to append audio (was {_state}).");
        }

        var bytesItem = BytesItem.CreateOwned(pcmData);
        _queue!.Push(bytesItem); // transfers ownership

        return default;
    }

    public async IAsyncEnumerable<LiveAudioTranscriptionResponse> GetStream(
        [EnumeratorCancellation] CancellationToken ct = default)
    {
        Detail.Throw.IfDisposed(_state == SessionState.Disposed, this);

        if (_state != SessionState.Started)
        {
            throw new FoundryLocalException($"Session must be Started to read stream (was {_state}).");
        }

        await foreach (var item in _channel!.Reader.ReadAllAsync(ct).ConfigureAwait(false))
        {
            yield return item;
        }
    }

    public async Task StopAsync(CancellationToken ct = default)
    {
        Detail.Throw.IfDisposed(_state == SessionState.Disposed, this);

        if (_state != SessionState.Started)
        {
            // Created (never started) or already Stopped — no-op.
            return;
        }

        // Signal end-of-input only. Do NOT cancel _stopCts here — the streaming callback
        // returns 1 (abort) when the stop token is signaled, which would tear down
        // ProcessRequest before it has drained queued audio and we'd lose the tail of
        // the transcription. Cancellation is reserved for DisposeAsync's abort path.
        _queue!.MarkFinished();

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
