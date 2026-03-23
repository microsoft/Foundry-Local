// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Runtime.CompilerServices;
using System.Globalization;
using System.Threading.Channels;
using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

/// <summary>
/// Session for real-time audio streaming ASR (Automatic Speech Recognition).
/// Audio data from a microphone (or other source) is pushed in as PCM chunks,
/// and transcription results are returned as an async stream.
///
/// Created via <see cref="OpenAIAudioClient.CreateLiveTranscriptionSession"/>.
///
/// Thread safety: AppendAsync can be called from any thread (including high-frequency
/// audio callbacks). Pushes are internally serialized via a bounded channel to prevent
/// unbounded memory growth and ensure ordering.
/// </summary>

public sealed class LiveAudioTranscriptionSession : IAsyncDisposable
{
    private readonly string _modelId;
    private readonly ICoreInterop _coreInterop = FoundryLocalManager.Instance.CoreInterop;
    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    // Session state — protected by _lock
    private readonly AsyncLock _lock = new();
    private string? _sessionHandle;
    private bool _started;
    private bool _stopped;

    // Output channel: native callback writes, user reads via GetTranscriptionStream
    private Channel<LiveAudioTranscriptionResponse>? _outputChannel;

    // Internal push queue: user writes audio chunks, background loop drains to native core.
    // Bounded to prevent unbounded memory growth if native core is slower than real-time.
    private Channel<ReadOnlyMemory<byte>>? _pushChannel;
    private Task? _pushLoopTask;

    // Dedicated CTS for the push loop — decoupled from StartAsync's caller token.
    // Cancelled only during StopAsync/DisposeAsync to allow clean drain.
    private CancellationTokenSource? _sessionCts;

    // Snapshot of settings captured at StartAsync — prevents mutation after session starts.
    private LiveAudioTranscriptionOptions? _activeSettings;

    /// <summary>
    /// Audio format settings for the streaming session.
    /// Must be configured before calling <see cref="StartAsync"/>.
    /// Settings are frozen once the session starts.
    /// </summary>
    public record LiveAudioTranscriptionOptions
    {
        /// <summary>PCM sample rate in Hz. Default: 16000.</summary>
        public int SampleRate { get; set; } = 16000;

        /// <summary>Number of audio channels. Default: 1 (mono).</summary>
        public int Channels { get; set; } = 1;

        /// <summary>Optional BCP-47 language hint (e.g., "en", "zh").</summary>
        public string? Language { get; set; }

        /// <summary>
        /// Maximum number of audio chunks buffered in the internal push queue.
        /// If the queue is full, AppendAsync will asynchronously wait.
        /// Default: 100 (~3 seconds of audio at typical chunk sizes).
        /// </summary>
        public int PushQueueCapacity { get; set; } = 100;

        internal LiveAudioTranscriptionOptions Snapshot() => this with { }; // record copy
    }

    public LiveAudioTranscriptionOptions Settings { get; } = new();

    internal LiveAudioTranscriptionSession(string modelId)
    {
        _modelId = modelId;
    }

    /// <summary>
    /// Start a real-time audio streaming session.
    /// Must be called before <see cref="AppendAsync"/> or <see cref="GetTranscriptionStream"/>.
    /// Settings are frozen after this call.
    /// </summary>
    /// <param name="ct">Cancellation token.</param>
    public async Task StartAsync(CancellationToken ct = default)
    {
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);

        if (_started)
        {
            throw new FoundryLocalException("Streaming session already started. Call StopAsync first.");
        }

        // Freeze settings
        _activeSettings = Settings.Snapshot();

        _outputChannel = Channel.CreateUnbounded<LiveAudioTranscriptionResponse>(
            new UnboundedChannelOptions
            {
                SingleWriter = true,  // only the native callback writes
                SingleReader = true,
                AllowSynchronousContinuations = true
            });

        _pushChannel = Channel.CreateBounded<ReadOnlyMemory<byte>>(
            new BoundedChannelOptions(_activeSettings.PushQueueCapacity)
            {
                SingleReader = true,   // only the push loop reads
                SingleWriter = false,  // multiple threads may push audio data
                FullMode = BoundedChannelFullMode.Wait
            });

        var request = new CoreInteropRequest
        {
            Params = new Dictionary<string, string>
            {
                { "Model", _modelId },
                { "SampleRate", _activeSettings.SampleRate.ToString(CultureInfo.InvariantCulture) },
                { "Channels", _activeSettings.Channels.ToString(CultureInfo.InvariantCulture) },
            }
        };

        if (_activeSettings.Language != null)
        {
            request.Params["Language"] = _activeSettings.Language;
        }

        // StartAudioStream uses existing execute_command entry point — synchronous P/Invoke
        var response = await Task.Run(
            () => _coreInterop.StartAudioStream(request), ct)
            .ConfigureAwait(false);

        if (response.Error != null)
        {
            _outputChannel.Writer.TryComplete();
            throw new FoundryLocalException(
                $"Error starting audio stream session: {response.Error}", _logger);
        }

        _sessionHandle = response.Data
            ?? throw new FoundryLocalException("Native core did not return a session handle.", _logger);
        _started = true;
        _stopped = false;

        _sessionCts = new CancellationTokenSource();
        _pushLoopTask = Task.Run(() => PushLoopAsync(_sessionCts.Token), CancellationToken.None);
    }

    /// <summary>
    /// Push a chunk of raw PCM audio data to the streaming session.
    /// Can be called from any thread (including audio device callbacks).
    /// Chunks are internally queued and serialized to the native core.
    /// </summary>
    /// <param name="pcmData">Raw PCM audio bytes matching the configured format.</param>
    /// <param name="ct">Cancellation token.</param>
    public async ValueTask AppendAsync(ReadOnlyMemory<byte> pcmData, CancellationToken ct = default)
    {
        if (!_started || _stopped)
        {
            throw new FoundryLocalException("No active streaming session. Call StartAsync first.");
        }

        // Copy the data to avoid issues if the caller reuses the buffer (e.g. NAudio reuses e.Buffer)
        var copy = new byte[pcmData.Length];
        pcmData.CopyTo(copy);

        await _pushChannel!.Writer.WriteAsync(copy, ct).ConfigureAwait(false);
    }

    /// <summary>
    /// Internal loop that drains the push queue and sends chunks to native core one at a time.
    /// Terminates the session on any native error.
    /// </summary>
    private async Task PushLoopAsync(CancellationToken ct)
    {
        try
        {
            await foreach (var audioData in _pushChannel!.Reader.ReadAllAsync(ct).ConfigureAwait(false))
            {
                var request = new CoreInteropRequest
                {
                    Params = new Dictionary<string, string> { { "SessionHandle", _sessionHandle! } }
                };

                var response = _coreInterop.PushAudioData(request, audioData);

                if (response.Error != null)
                {
                    var errorInfo = CoreErrorResponse.TryParse(response.Error);
                    var fatalEx = new FoundryLocalException(
                        $"Push failed (code={errorInfo?.Code ?? "UNKNOWN"}): {response.Error}",
                        _logger);
                    _logger.LogError("Terminating push loop due to push failure: {Error}",
                                     response.Error);
                    _outputChannel?.Writer.TryComplete(fatalEx);
                    return;
                }

                // Parse transcription result from push response and surface it
                if (!string.IsNullOrEmpty(response.Data))
                {
                    try
                    {
                        var transcription = LiveAudioTranscriptionResponse.FromJson(response.Data);
                        if (!string.IsNullOrEmpty(transcription.Text))
                        {
                            _outputChannel?.Writer.TryWrite(transcription);
                        }
                    }
                    catch (Exception parseEx)
                    {
                        // Non-fatal: log and continue if response isn't a transcription result
                        _logger.LogDebug(parseEx, "Could not parse push response as transcription result");
                    }
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Expected on cancellation — push loop exits cleanly
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Push loop terminated with unexpected error");
            _outputChannel?.Writer.TryComplete(
                new FoundryLocalException("Push loop terminated unexpectedly.", ex, _logger));
        }
    }

    /// <summary>
    /// Get the async stream of transcription results.
    /// Results arrive as the native ASR engine processes audio data.
    /// </summary>
    /// <param name="ct">Cancellation token.</param>
    /// <returns>Async enumerable of transcription results.</returns>
    public async IAsyncEnumerable<LiveAudioTranscriptionResponse> GetTranscriptionStream(
        [EnumeratorCancellation] CancellationToken ct = default)
    {
        if (_outputChannel == null)
        {
            throw new FoundryLocalException("No active streaming session. Call StartAsync first.");
        }

        await foreach (var item in _outputChannel.Reader.ReadAllAsync(ct).ConfigureAwait(false))
        {
            yield return item;
        }
    }

    /// <summary>
    /// Signal end-of-audio and stop the streaming session.
    /// Any remaining buffered audio in the push queue will be drained to native core first.
    /// Final results are delivered through <see cref="GetTranscriptionStream"/> before it completes.
    /// </summary>
    /// <param name="ct">Cancellation token.</param>
    public async Task StopAsync(CancellationToken ct = default)
    {
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);

        if (!_started || _stopped)
        {
            return; // already stopped or never started
        }

        _stopped = true;

        // 1. Complete the push channel so the push loop drains remaining items and exits
        _pushChannel?.Writer.TryComplete();

        // 2. Wait for the push loop to finish draining
        if (_pushLoopTask != null)
        {
            await _pushLoopTask.ConfigureAwait(false);
        }

        // 3. Cancel the session CTS (no-op if push loop already exited)
        _sessionCts?.Cancel();

        // 4. Tell native core to flush and finalize.
        //    This MUST happen even if ct is cancelled — otherwise native session leaks.
        var request = new CoreInteropRequest
        {
            Params = new Dictionary<string, string> { { "SessionHandle", _sessionHandle! } }
        };

        ICoreInterop.Response? response = null;
        try
        {
            response = await Task.Run(
                () => _coreInterop.StopAudioStream(request), ct)
                .ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            // ct fired, but we MUST still stop the native session to avoid a leak.
            _logger.LogWarning("StopAsync cancelled — performing best-effort native session stop.");
            try
            {
                response = await Task.Run(
                    () => _coreInterop.StopAudioStream(request))
                    .ConfigureAwait(false);
            }
            catch (Exception cleanupEx)
            {
                _logger.LogError(cleanupEx, "Best-effort native session stop failed.");
            }

            throw; // Re-throw the cancellation after cleanup
        }
        finally
        {
            // Parse final transcription from stop response before completing the channel
            if (response?.Data != null)
            {
                try
                {
                    var finalResult = LiveAudioTranscriptionResponse.FromJson(response.Data);
                    if (!string.IsNullOrEmpty(finalResult.Text))
                    {
                        _outputChannel?.Writer.TryWrite(finalResult);
                    }
                }
                catch (Exception parseEx)
                {
                    _logger.LogDebug(parseEx, "Could not parse stop response as transcription result");
                }
            }

            _sessionHandle = null;
            _started = false;
            _sessionCts?.Dispose();
            _sessionCts = null;

            // Complete the output channel AFTER writing final result
            _outputChannel?.Writer.TryComplete();
        }

        if (response?.Error != null)
        {
            throw new FoundryLocalException(
                $"Error stopping audio stream session: {response.Error}", _logger);
        }
    }

    /// <summary>
    /// Dispose the streaming session. Calls <see cref="StopAsync"/> if the session is still active.
    /// Safe to call multiple times.
    /// </summary>
    public async ValueTask DisposeAsync()
    {
        try
        {
            if (_started && !_stopped)
            {
                await StopAsync().ConfigureAwait(false);
            }
        }
        catch (Exception ex)
        {
            // DisposeAsync must never throw — log and swallow
            _logger.LogWarning(ex, "Error during DisposeAsync cleanup.");
        }
        finally
        {
            _sessionCts?.Dispose();
            _lock.Dispose();
        }
    }
}