import { CoreInterop } from '../detail/coreInterop.js';
import { LiveAudioTranscriptionResult, parseTranscriptionResult, tryParseCoreError } from './liveAudioTranscriptionTypes.js';

/**
 * Audio format settings for a streaming session.
 * Must be configured before calling start().
 * Settings are frozen once the session starts.
 */
export class LiveAudioTranscriptionSettings {
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
    snapshot(): LiveAudioTranscriptionSettings {
        const copy = new LiveAudioTranscriptionSettings();
        copy.sampleRate = this.sampleRate;
        copy.channels = this.channels;
        copy.bitsPerSample = this.bitsPerSample;
        copy.language = this.language;
        copy.pushQueueCapacity = this.pushQueueCapacity;
        return Object.freeze(copy) as LiveAudioTranscriptionSettings;
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

        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: item, done: false });
            return;
        }

        if (this.queue.length >= this.maxCapacity) {
            await new Promise<void>((resolve) => {
                this.backpressureResolve = resolve;
            });
        }

        this.queue.push(item);
    }

    /** Push an item synchronously (no backpressure wait). */
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

        if (this.backpressureResolve) {
            this.backpressureResolve();
            this.backpressureResolve = null;
        }

        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: undefined as any, done: true });
        }
    }

    get error(): Error | null {
        return this.completionError;
    }

    /** Async iterator for consuming items. */
    async *[Symbol.asyncIterator](): AsyncGenerator<T> {
        while (true) {
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
 * and transcription results are returned as an async iterable.
 *
 * Mirrors the C# LiveAudioTranscriptionSession.
 */
export class LiveAudioTranscriptionClient {
    private modelId: string;
    private coreInterop: CoreInterop;

    private sessionHandle: string | null = null;
    private started = false;
    private stopped = false;

    private outputQueue: AsyncQueue<LiveAudioTranscriptionResult> | null = null;
    private pushQueue: AsyncQueue<Uint8Array> | null = null;
    private pushLoopPromise: Promise<void> | null = null;
    private activeSettings: LiveAudioTranscriptionSettings | null = null;
    private sessionAbortController: AbortController | null = null;

    /**
     * Configuration settings for the streaming session.
     * Must be configured before calling start(). Settings are frozen after start().
     */
    public settings = new LiveAudioTranscriptionSettings();

    /**
     * @internal
     * Users should create clients via Model.createLiveTranscriptionClient().
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

        this.activeSettings = this.settings.snapshot();
        this.outputQueue = new AsyncQueue<LiveAudioTranscriptionResult>();
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

        try {
            const response = this.coreInterop.executeCommand("audio_stream_start", {
                Params: params
            });

            this.sessionHandle = response;
            if (!this.sessionHandle) {
                throw new Error('Native core did not return a session handle.');
            }
        } catch (error) {
            const err = new Error(
                `Error starting audio stream session: ${error instanceof Error ? error.message : String(error)}`,
                { cause: error }
            );
            this.outputQueue.complete(err);
            throw err;
        }

        this.started = true;
        this.stopped = false;

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

        const copy = new Uint8Array(pcmData.length);
        copy.set(pcmData);

        await this.pushQueue!.write(copy);
    }

    /**
     * Internal loop that drains the push queue and sends chunks to native core one at a time.
     * Terminates the session on any native error.
     * @internal
     */
    private async pushLoop(): Promise<void> {
        try {
            for await (const audioData of this.pushQueue!) {
                if (this.sessionAbortController?.signal.aborted) {
                    break;
                }

                try {
                    const responseData = this.coreInterop.executeCommandWithBinary("audio_stream_push", {
                        Params: {
                            SessionHandle: this.sessionHandle!,
                        }
                    }, audioData);

                    // Parse transcription result from push response and surface it
                    if (responseData) {
                        try {
                            const result = parseTranscriptionResult(responseData);
                            const text = result.content?.[0]?.text;
                            if (text !== undefined && text !== null && text !== '') {
                                this.outputQueue?.tryWrite(result);
                            }
                        } catch {
                            // Non-fatal: log and continue if response isn't a transcription result
                        }
                    }
                } catch (error) {
                    const errorMsg = error instanceof Error ? error.message : String(error);
                    const errorInfo = tryParseCoreError(errorMsg);

                    const fatalError = new Error(
                        `Push failed (code=${errorInfo?.code ?? 'UNKNOWN'}): ${errorMsg}`,
                        { cause: error }
                    );
                    console.error('Terminating push loop due to push failure:', errorMsg);
                    this.outputQueue?.complete(fatalError);
                    return;
                }
            }
        } catch (error) {
            if (this.sessionAbortController?.signal.aborted) {
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
     *     console.log(result.content[0].text);
     * }
     * ```
     */
    public async *getTranscriptionStream(): AsyncGenerator<LiveAudioTranscriptionResult> {
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
            return;
        }

        this.stopped = true;

        this.pushQueue?.complete();

        if (this.pushLoopPromise) {
            await this.pushLoopPromise;
        }

        this.sessionAbortController?.abort();

        let stopError: Error | null = null;
        try {
            const responseData = this.coreInterop.executeCommand("audio_stream_stop", {
                Params: { SessionHandle: this.sessionHandle! }
            });

            // Parse final transcription from stop response
            if (responseData) {
                try {
                    const finalResult = parseTranscriptionResult(responseData);
                    if (finalResult.content?.[0]?.text) {
                        this.outputQueue?.tryWrite(finalResult);
                    }
                } catch {
                    // Non-fatal
                }
            }
        } catch (error) {
            stopError = error instanceof Error ? error : new Error(String(error));
            console.error('Error stopping audio stream session:', stopError.message);
        }

        this.sessionHandle = null;
        this.started = false;
        this.sessionAbortController = null;

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
            console.warn('Error during dispose cleanup:', error instanceof Error ? error.message : String(error));
        }
    }
}
