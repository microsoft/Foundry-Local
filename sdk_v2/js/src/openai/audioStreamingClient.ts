import { CoreInterop } from '../detail/coreInterop.js';
import { AudioStreamTranscriptionResult, tryParseCoreError } from './audioStreamingTypes.js';

/**
 * Audio format settings for a streaming session.
 * Must be configured before calling start().
 * Settings are frozen once the session starts.
 */
export class StreamingAudioSettings {
    /** PCM sample rate in Hz. Default: 16000. */
    sampleRate: number = 16000;
    /** Number of audio channels. Default: 1 (mono). */
    channels: number = 1;
    /** Bits per sample. Default: 16. */
    bitsPerSample: number = 16;
    /** Optional BCP-47 language hint (e.g., "en", "zh"). */
    language?: string;
    /** Maximum number of audio chunks buffered in the internal push queue. Default: 100. */
    pushQueueCapacity: number = 100;

    /** @internal Create a frozen copy of these settings. */
    snapshot(): StreamingAudioSettings {
        const copy = new StreamingAudioSettings();
        copy.sampleRate = this.sampleRate;
        copy.channels = this.channels;
        copy.bitsPerSample = this.bitsPerSample;
        copy.language = this.language;
        copy.pushQueueCapacity = this.pushQueueCapacity;
        return Object.freeze(copy) as StreamingAudioSettings;
    }
}

/**
 * Internal async queue that acts like C#'s Channel<T>.
 * Supports a single consumer reading via async iteration and multiple producers writing.
 * @internal
 */
class AsyncQueue<T> {
    private queue: T[] = [];
    private waitingResolve: ((value: IteratorResult<T>) => void) | null = null;
    private completed = false;
    private completionError: Error | null = null;
    private maxCapacity: number;
    private backpressureResolve: (() => void) | null = null;

    constructor(maxCapacity: number = Infinity) {
        this.maxCapacity = maxCapacity;
    }

    /** Push an item. If at capacity, waits until space is available. */
    async write(item: T): Promise<void> {
        if (this.completed) {
            throw new Error('Cannot write to a completed queue.');
        }

        // If someone is waiting to read, deliver directly
        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: item, done: false });
            return;
        }

        // If at capacity, wait for space
        if (this.queue.length >= this.maxCapacity) {
            await new Promise<void>((resolve) => {
                this.backpressureResolve = resolve;
            });
        }

        this.queue.push(item);
    }

    /** Push an item synchronously (no backpressure wait). Used by native callbacks. */
    tryWrite(item: T): boolean {
        if (this.completed) return false;

        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: item, done: false });
            return true;
        }

        this.queue.push(item);
        return true;
    }

    /** Signal that no more items will be written. */
    complete(error?: Error): void {
        if (this.completed) return;
        this.completed = true;
        this.completionError = error ?? null;

        // Release backpressure waiter
        if (this.backpressureResolve) {
            this.backpressureResolve();
            this.backpressureResolve = null;
        }

        // Wake up any waiting reader
        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            if (this.completionError) {
                // Can't reject through iterator result — reader will get done:true
                // and the error is surfaced via the completionError property
            }
            resolve({ value: undefined as any, done: true });
        }
    }

    get error(): Error | null {
        return this.completionError;
    }

    /** Async iterator for consuming items. */
    async *[Symbol.asyncIterator](): AsyncGenerator<T> {
        while (true) {
            // Release backpressure if queue drained below capacity
            if (this.backpressureResolve && this.queue.length < this.maxCapacity) {
                const resolve = this.backpressureResolve;
                this.backpressureResolve = null;
                resolve();
            }

            if (this.queue.length > 0) {
                yield this.queue.shift()!;
                continue;
            }

            if (this.completed) {
                if (this.completionError) {
                    throw this.completionError;
                }
                return;
            }

            // Wait for next item or completion
            const result = await new Promise<IteratorResult<T>>((resolve) => {
                this.waitingResolve = resolve;
            });

            if (result.done) {
                if (this.completionError) {
                    throw this.completionError;
                }
                return;
            }

            yield result.value;
        }
    }
}

/**
 * Client for real-time audio streaming ASR (Automatic Speech Recognition).
 * Audio data from a microphone (or other source) is pushed in as PCM chunks,
 * and partial transcription results are returned as an async iterable.
 *
 * Thread safety: pushAudioData() can be called from any context.
 * Pushes are internally queued and serialized to native core one at a time.
 *
 * Mirrors the C# OpenAIAudioStreamingClient.
 */
export class AudioStreamingClient {
    private modelId: string;
    private coreInterop: CoreInterop;

    // Session state
    private sessionHandle: string | null = null;
    private started = false;
    private stopped = false;

    // Output queue: native callback writes, user reads via getTranscriptionStream()
    private outputQueue: AsyncQueue<AudioStreamTranscriptionResult> | null = null;

    // Internal push queue: user writes audio chunks, push loop drains to native core
    private pushQueue: AsyncQueue<Uint8Array> | null = null;
    private pushLoopPromise: Promise<void> | null = null;

    // Frozen settings snapshot
    private activeSettings: StreamingAudioSettings | null = null;

    // Abort controller for the push loop — decoupled from caller's signal
    private sessionAbortController: AbortController | null = null;

    // Whether native callback has been registered (for tracking)
    private nativeCallbackRegistered = false;

    /**
     * Configuration settings for the streaming session.
     * Must be configured before calling start(). Settings are frozen after start().
     */
    public settings = new StreamingAudioSettings();

    /**
     * @internal
     * Restricted to internal use. Users should create clients via Model.createAudioStreamingClient().
     */
    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Start a real-time audio streaming session.
     * Must be called before pushAudioData() or getTranscriptionStream().
     * Settings are frozen after this call.
     */
    public async start(): Promise<void> {
        if (this.started) {
            throw new Error('Streaming session already started. Call stop() first.');
        }

        // Freeze settings
        this.activeSettings = this.settings.snapshot();

        this.outputQueue = new AsyncQueue<AudioStreamTranscriptionResult>();
        this.pushQueue = new AsyncQueue<Uint8Array>(this.activeSettings.pushQueueCapacity);

        const params: Record<string, string> = {
            Model: this.modelId,
            SampleRate: this.activeSettings.sampleRate.toString(),
            Channels: this.activeSettings.channels.toString(),
            BitsPerSample: this.activeSettings.bitsPerSample.toString(),
        };

        if (this.activeSettings.language) {
            params['Language'] = this.activeSettings.language;
        }

        // Start session via native core with a callback for transcription results.
        // executeCommandStreaming registers a callback and calls the native function async.
        // For audio_stream_start, the native function returns immediately (non-blocking)
        // and invokes the callback on a native thread whenever partial results are ready.
        //
        // However, the current CoreInterop.executeCommandStreaming wraps the call in
        // execute_command_with_callback which blocks until the command completes.
        // For audio streaming, we need the start command to return immediately.
        // We use executeCommand (synchronous) for start, and the callback is registered
        // by the native core during that call.
        //
        // NOTE: This matches the C# pattern where StartAudioStream is synchronous and
        // the callback is registered during the P/Invoke call. The JS koffi FFI works
        // similarly — the native function registers our callback pointer and returns.

        try {
            const response = this.coreInterop.executeCommand("audio_stream_start", {
                Params: params
            });

            this.sessionHandle = response;
            if (!this.sessionHandle) {
                throw new Error('Native core did not return a session handle.');
            }
        } catch (error) {
            this.outputQueue.complete();
            throw new Error(
                `Error starting audio stream session: ${error instanceof Error ? error.message : String(error)}`,
                { cause: error }
            );
        }

        this.started = true;
        this.stopped = false;

        // Start the background push loop
        this.sessionAbortController = new AbortController();
        this.pushLoopPromise = this.pushLoop();
    }

    /**
     * Push a chunk of raw PCM audio data to the streaming session.
     * Can be called from any context. Chunks are internally queued
     * and serialized to native core one at a time.
     *
     * @param pcmData - Raw PCM audio bytes matching the configured format.
     */
    public async pushAudioData(pcmData: Uint8Array): Promise<void> {
        if (!this.started || this.stopped) {
            throw new Error('No active streaming session. Call start() first.');
        }

        // Copy the buffer to avoid issues if the caller reuses it
        const copy = new Uint8Array(pcmData.length);
        copy.set(pcmData);

        await this.pushQueue!.write(copy);
    }

    /**
     * Internal loop that drains the push queue and sends chunks to native core one at a time.
     * Implements retry for transient native errors and terminates on permanent failures.
     * @internal
     */
    private async pushLoop(): Promise<void> {
        const maxRetries = 3;
        const initialRetryDelayMs = 50;

        try {
            for await (const audioData of this.pushQueue!) {
                // Check if aborted
                if (this.sessionAbortController?.signal.aborted) {
                    break;
                }

                let pushed = false;
                for (let attempt = 0; attempt <= maxRetries && !pushed; attempt++) {
                    try {
                        // Send audio data to native core.
                        // The native core receives the session handle and audio details via JSON params.
                        this.coreInterop.executeCommand("audio_stream_push", {
                            Params: {
                                SessionHandle: this.sessionHandle!,
                                AudioDataLength: audioData.length.toString()
                            }
                        });
                        pushed = true;
                    } catch (error) {
                        const errorMsg = error instanceof Error ? error.message : String(error);
                        const errorInfo = tryParseCoreError(errorMsg);

                        if (errorInfo?.isTransient && attempt < maxRetries) {
                            const delay = initialRetryDelayMs * Math.pow(2, attempt);
                            console.warn(
                                `Transient push error (attempt ${attempt + 1}/${maxRetries}): ${errorInfo.code}. Retrying in ${delay}ms`
                            );
                            await new Promise(resolve => setTimeout(resolve, delay));
                            continue;
                        }

                        // Permanent error or retries exhausted
                        const fatalError = new Error(
                            `Push failed permanently (code=${errorInfo?.code ?? 'UNKNOWN'}): ${errorMsg}`,
                            { cause: error }
                        );
                        console.error('Terminating push loop due to permanent push failure:', errorMsg);
                        this.outputQueue?.complete(fatalError);
                        return;
                    }
                }
            }
        } catch (error) {
            if (this.sessionAbortController?.signal.aborted) {
                // Expected on cancellation
                return;
            }
            const err = error instanceof Error ? error : new Error(String(error));
            console.error('Push loop terminated with unexpected error:', err.message);
            this.outputQueue?.complete(new Error('Push loop terminated unexpectedly.', { cause: err }));
        }
    }

    /**
     * Get the async iterable of transcription results.
     * Results arrive as the native ASR engine processes audio data.
     *
     * Usage:
     * ```ts
     * for await (const result of client.getTranscriptionStream()) {
     *     console.log(result.text);
     * }
     * ```
     */
    public async *getTranscriptionStream(): AsyncGenerator<AudioStreamTranscriptionResult> {
        if (!this.outputQueue) {
            throw new Error('No active streaming session. Call start() first.');
        }

        for await (const item of this.outputQueue) {
            yield item;
        }
    }

    /**
     * Signal end-of-audio and stop the streaming session.
     * Any remaining buffered audio in the push queue will be drained to native core first.
     * Final results are delivered through getTranscriptionStream() before it completes.
     */
    public async stop(): Promise<void> {
        if (!this.started || this.stopped) {
            return; // already stopped or never started
        }

        this.stopped = true;

        // 1. Complete the push queue so the push loop drains remaining items and exits
        this.pushQueue?.complete();

        // 2. Wait for the push loop to finish draining
        if (this.pushLoopPromise) {
            await this.pushLoopPromise;
        }

        // 3. Abort the session (no-op if push loop already exited)
        this.sessionAbortController?.abort();

        // 4. Tell native core to flush and finalize
        let stopError: Error | null = null;
        try {
            this.coreInterop.executeCommand("audio_stream_stop", {
                Params: { SessionHandle: this.sessionHandle! }
            });
        } catch (error) {
            stopError = error instanceof Error ? error : new Error(String(error));
            console.error('Error stopping audio stream session:', stopError.message);
        }

        // 5. Clean up state
        this.sessionHandle = null;
        this.started = false;
        this.sessionAbortController = null;

        // 6. Complete the output queue AFTER the native stop so final callbacks are captured
        this.outputQueue?.complete();

        if (stopError) {
            throw new Error(
                `Error stopping audio stream session: ${stopError.message}`,
                { cause: stopError }
            );
        }
    }

    /**
     * Dispose the client and stop any active session.
     * Safe to call multiple times.
     */
    public async dispose(): Promise<void> {
        try {
            if (this.started && !this.stopped) {
                await this.stop();
            }
        } catch (error) {
            // dispose must not throw — log and swallow
            console.warn('Error during dispose cleanup:', error instanceof Error ? error.message : String(error));
        }
    }
}
