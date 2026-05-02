import { CoreInterop } from '../detail/coreInterop.js';
import { LiveAudioTranscriptionResponse, parseTranscriptionResult, wrapCoreError } from './liveAudioTranscriptionTypes.js';

/**
 * Audio format settings for a streaming session.
 * Must be configured before calling start().
 * Settings are frozen once the session starts.
 */
export class LiveAudioTranscriptionOptions {
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
    snapshot(): LiveAudioTranscriptionOptions {
        const copy = new LiveAudioTranscriptionOptions();
        copy.sampleRate = this.sampleRate;
        copy.channels = this.channels;
        copy.bitsPerSample = this.bitsPerSample;
        copy.language = this.language;
        copy.pushQueueCapacity = this.pushQueueCapacity;
        return Object.freeze(copy) as LiveAudioTranscriptionOptions;
    }
}

/**
 * DOMException-compatible AbortError. Matches the shape thrown by native fetch/AbortController
 * so callers can use `err.name === 'AbortError'` for cancellation detection.
 * @internal
 */
function makeAbortError(message = 'The operation was aborted.'): Error {
    const err = new Error(message);
    err.name = 'AbortError';
    return err;
}

/**
 * Convert an AbortSignal's `reason` into a human-readable message.
 * Handles all three cases: Error reasons, non-Error reasons (e.g.,
 * `controller.abort('timeout')`), and undefined reasons.
 * @internal
 */
function abortMessage(signal: AbortSignal): string {
    const reason = signal.reason;
    if (reason instanceof Error) return reason.message;
    if (reason !== undefined) return String(reason);
    return 'The operation was aborted.';
}

/**
 * If `signal` is already aborted, throw an AbortError immediately.
 * @internal
 */
function throwIfAborted(signal: AbortSignal | undefined): void {
    if (signal?.aborted) {
        throw makeAbortError(abortMessage(signal));
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
    private backpressureQueue: (() => void)[] = [];

    constructor(maxCapacity: number = Infinity) {
        this.maxCapacity = maxCapacity;
    }

    /**
     * Push an item. If at capacity, waits until space is available.
     *
     * @param item - The value to enqueue.
     * @param signal - Optional AbortSignal. If aborted while waiting on
     *                 backpressure, the waiter is removed from the queue and
     *                 an AbortError is thrown. The item is NOT enqueued.
     */
    async write(item: T, signal?: AbortSignal): Promise<void> {
        if (this.completed) {
            throw new Error('Cannot write to a completed queue.');
        }
        if (signal?.aborted) {
            throw makeAbortError(abortMessage(signal));
        }

        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: item, done: false });
            return;
        }

        while (this.queue.length >= this.maxCapacity) {
            // Make backpressure wait abort-aware: if the signal fires, remove
            // our resolver from backpressureQueue so the chunk is never enqueued.
            let waiterResolve!: () => void;
            const waiter = new Promise<void>((resolve) => {
                waiterResolve = resolve;
                this.backpressureQueue.push(resolve);
            });

            if (!signal) {
                await waiter;
            } else {
                let onAbort: (() => void) | null = null;
                const abortPromise = new Promise<never>((_, reject) => {
                    onAbort = () => reject(makeAbortError(abortMessage(signal)));
                    signal.addEventListener('abort', onAbort, { once: true });
                });
                try {
                    await Promise.race([waiter, abortPromise]);
                } catch (err) {
                    // Aborted while backpressured — drop our resolver from the queue
                    // so we don't get woken up later and (worse) silently enqueue
                    // the item the caller already saw rejected.
                    const idx = this.backpressureQueue.indexOf(waiterResolve);
                    if (idx !== -1) this.backpressureQueue.splice(idx, 1);
                    throw err;
                } finally {
                    if (onAbort) signal.removeEventListener('abort', onAbort);
                }
            }
        }

        if (this.completed) {
            throw new Error('Cannot write to a completed queue.');
        }
        if (signal?.aborted) {
            throw makeAbortError(abortMessage(signal));
        }

        this.queue.push(item);
    }

    /** Push an item synchronously (no backpressure wait). Returns false if completed or at capacity. */
    tryWrite(item: T): boolean {
        if (this.completed) return false;

        if (this.waitingResolve) {
            const resolve = this.waitingResolve;
            this.waitingResolve = null;
            resolve({ value: item, done: false });
            return true;
        }

        if (this.queue.length >= this.maxCapacity) {
            return false;
        }

        this.queue.push(item);
        return true;
    }

    /** Signal that no more items will be written. */
    complete(error?: Error): void {
        if (this.completed) return;
        this.completed = true;
        this.completionError = error ?? null;

        // Release all blocked writers
        for (const resolve of this.backpressureQueue) {
            resolve();
        }
        this.backpressureQueue = [];

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
            if (this.backpressureQueue.length > 0 && this.queue.length < this.maxCapacity) {
                const resolve = this.backpressureQueue.shift()!;
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
export class LiveAudioTranscriptionSession {
    private modelId: string;
    private coreInterop: CoreInterop;

    private sessionHandle: string | null = null;
    private started = false;
    private stopped = false;

    private outputQueue: AsyncQueue<LiveAudioTranscriptionResponse> | null = null;
    private pushQueue: AsyncQueue<Uint8Array> | null = null;
    private pushLoopPromise: Promise<void> | null = null;
    private activeSettings: LiveAudioTranscriptionOptions | null = null;
    private sessionAbortController: AbortController | null = null;
    private streamConsumed = false;

    /**
     * Configuration settings for the streaming session.
     * Must be configured before calling start(). Settings are snapshotted at start();
     * changes made after start() are ignored for the current session.
     */
    public settings = new LiveAudioTranscriptionOptions();

    /**
     * @internal
     * Users should create sessions via AudioClient.createLiveTranscriptionSession().
     */
    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Start a real-time audio streaming session.
     * Must be called before append() or getTranscriptionStream().
     * Settings are frozen after this call.
     *
     * @param signal - Optional AbortSignal. If already aborted when start() is
     *                 called, an AbortError is thrown and no native session is
     *                 created. The signal is also wired into the session for the
     *                 lifetime of the call so that aborting later short-circuits
     *                 append() / getTranscriptionStream() (see those methods).
     *                 (Note: start() itself runs synchronously up to the native
     *                 call, so an abort signaled during start() cannot interrupt
     *                 it; the signal takes effect on the next async boundary.)
     */
    public async start(signal?: AbortSignal): Promise<void> {
        if (this.started) {
            throw new Error('Streaming session already started. Call stop() first.');
        }
        throwIfAborted(signal);

        this.activeSettings = this.settings.snapshot();
        this.outputQueue = new AsyncQueue<LiveAudioTranscriptionResponse>();
        this.pushQueue = new AsyncQueue<Uint8Array>(this.activeSettings.pushQueueCapacity);
        this.streamConsumed = false;

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
            const err = wrapCoreError('Error starting audio stream session: ', error);
            this.outputQueue.complete(err);
            throw err;
        }

        this.started = true;
        this.stopped = false;

        this.sessionAbortController = new AbortController();
        if (signal) {
            // throwIfAborted() at the top already handled pre-aborted signals
            // and start() is synchronous through here, so signal cannot have
            // fired between those two points. Just wire the listener.
            //
            // Use AbortSignal.any-style auto-removal: when our internal
            // sessionAbortController fires (in stop()/handleExternalAbort),
            // the listener is removed automatically. This avoids a memory
            // leak where a long-lived caller signal kept the session
            // instance alive via the closure capturing `this` after the
            // session ended normally.
            signal.addEventListener('abort', () => this.handleExternalAbort(signal), {
                once: true,
                signal: this.sessionAbortController.signal,
            });
        }
        this.pushLoopPromise = this.pushLoop();
    }

    /**
     * Handle an external AbortSignal firing while the session is active.
     * Tears down the session by completing internal queues with an AbortError,
     * and best-effort releases the native session handle.
     * @internal
     */
    private handleExternalAbort(signal: AbortSignal): void {
        if (this.stopped || !this.started) return;
        const err = makeAbortError(abortMessage(signal));
        this.stopped = true;
        this.started = false;
        this.sessionAbortController?.abort();
        this.pushQueue?.complete(err);
        this.outputQueue?.complete(err);

        // Best-effort release of the native session handle. Without this the
        // native core leaks a session per aborted client.
        const handle = this.sessionHandle;
        this.sessionHandle = null;
        if (handle) {
            try {
                this.coreInterop.executeCommand("audio_stream_stop", {
                    Params: { SessionHandle: handle }
                });
            } catch {
                // Swallow: the session is already torn down on our side and
                // we've surfaced the abort to the caller.
            }
        }
    }

    /**
     * Push a chunk of raw PCM audio data to the streaming session.
     * Can be called from any context. Chunks are internally queued
     * and serialized to native core one at a time.
     *
     * @param pcmData - Raw PCM audio bytes matching the configured format.
     * @param signal - Optional AbortSignal. If aborted while waiting for queue
     *                 capacity, an AbortError is thrown and the chunk is NOT
     *                 enqueued (no risk of late delivery to native core).
     */
    public async append(pcmData: Uint8Array, signal?: AbortSignal): Promise<void> {
        if (!this.started || this.stopped) {
            throw new Error('No active streaming session. Call start() first.');
        }
        throwIfAborted(signal);

        const copy = new Uint8Array(pcmData.length);
        copy.set(pcmData);

        // AsyncQueue.write is abort-aware: on abort, the backpressure waiter
        // is removed and AbortError is thrown without enqueuing the chunk.
        await this.pushQueue!.write(copy, signal);
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
                    const fatalError = wrapCoreError(`Push failed: `, error);
                    // Preserve the previous "Push failed (code=...)" prefix in the message for log compatibility.
                    (fatalError as { message: string }).message = `Push failed (code=${fatalError.code}): ${errorMsg}`;
                    this.stopped = true;
                    this.started = false;
                    this.pushQueue?.complete(fatalError);
                    this.outputQueue?.complete(fatalError);
                    return;
                }
            }
        } catch (error) {
            if (this.sessionAbortController?.signal.aborted) {
                return;
            }
            const err = error instanceof Error ? error : new Error(String(error));
            this.outputQueue?.complete(new Error('Push loop terminated unexpectedly.', { cause: err }));
        }
    }

    /**
     * Get the async iterable of transcription results.
     * Results arrive as the native ASR engine processes audio data.
     *
     * @param signal - Optional AbortSignal. If aborted, iteration ends with an AbortError.
     *
     * Usage:
     * ```ts
     * for await (const result of client.getTranscriptionStream()) {
     *     console.log(result.content[0].text);
     * }
     * ```
     */
    public async *getTranscriptionStream(signal?: AbortSignal): AsyncGenerator<LiveAudioTranscriptionResponse> {
        if (!this.outputQueue) {
            throw new Error('No active streaming session. Call start() first.');
        }
        if (this.streamConsumed) {
            throw new Error('getTranscriptionStream() can only be called once per session. The output stream has already been consumed.');
        }
        // Check abort BEFORE marking the stream consumed so a pre-aborted
        // signal doesn't permanently disable the (single-use) stream.
        throwIfAborted(signal);
        this.streamConsumed = true;

        // If a signal is provided, complete the output queue with an AbortError on abort
        // so the pending iterator yield rejects promptly.
        const queue = this.outputQueue;
        let onAbort: (() => void) | null = null;
        if (signal) {
            onAbort = () => queue.complete(makeAbortError(abortMessage(signal)));
            signal.addEventListener('abort', onAbort, { once: true });
        }

        try {
            for await (const item of queue) {
                yield item;
            }
        } finally {
            if (signal && onAbort) {
                signal.removeEventListener('abort', onAbort);
            }
        }
    }

    /**
     * Signal end-of-audio and stop the streaming session.
     * Any remaining buffered audio in the push queue will be drained to native core first.
     * Final results are delivered through getTranscriptionStream() before it completes.
     *
     * @param signal - Optional AbortSignal. If aborted while draining the push queue, drain is
     *                 short-circuited and the native session is stopped immediately.
     */
    public async stop(signal?: AbortSignal): Promise<void> {
        if (!this.started || this.stopped) {
            return;
        }

        this.stopped = true;

        this.pushQueue?.complete();

        if (this.pushLoopPromise) {
            if (signal) {
                // Allow the caller to short-circuit the drain via abort.
                let onAbort: (() => void) | null = null;
                const abortPromise = new Promise<void>((resolve) => {
                    onAbort = () => {
                        this.sessionAbortController?.abort();
                        resolve();
                    };
                    if (signal.aborted) {
                        // addEventListener doesn't fire on already-aborted signals.
                        onAbort();
                    } else {
                        signal.addEventListener('abort', onAbort, { once: true });
                    }
                });
                try {
                    await Promise.race([this.pushLoopPromise, abortPromise]);
                } finally {
                    if (onAbort && !signal.aborted) signal.removeEventListener('abort', onAbort);
                }
            } else {
                await this.pushLoopPromise;
            }
        }

        this.sessionAbortController?.abort();

        let stopError: unknown = null;
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
            stopError = error;
        }

        this.sessionHandle = null;
        this.started = false;
        this.sessionAbortController = null;

        this.outputQueue?.complete();

        if (stopError) {
            throw wrapCoreError('Error stopping audio stream session: ', stopError);
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
        } catch {
            // Swallow errors during best-effort cleanup to keep dispose() silent.
        }
    }
}
