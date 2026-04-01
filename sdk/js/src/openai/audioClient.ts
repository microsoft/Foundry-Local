import { CoreInterop } from '../detail/coreInterop.js';
import { LiveAudioTranscriptionClient } from './liveAudioTranscriptionClient.js';

export class AudioClientSettings {
    language?: string;
    temperature?: number;

    /**
     * Serializes the settings into an OpenAI-compatible request object.
     * @internal
     */
    _serialize() {
        // Standard OpenAI properties
        const result: any = {
            Language: this.language,
            Temperature: this.temperature,
        };

        // Foundry specific metadata properties
        const metadata: Record<string, string> = {};
        if (this.language !== undefined) {
          metadata["language"] = this.language;
        }
        if (this.temperature !== undefined) {
            metadata["temperature"] = this.temperature.toString();
        }
        
        if (Object.keys(metadata).length > 0) {
            result.metadata = metadata;
        }

        // Filter out undefined properties
        return Object.fromEntries(Object.entries(result).filter(([_, v]) => v !== undefined));
    }
}

/**
 * Client for performing audio operations (transcription, translation) with a loaded model.
 * Follows the OpenAI Audio API structure.
 */
export class AudioClient {
    private modelId: string;
    private coreInterop: CoreInterop;
    
    /**
     * Configuration settings for audio operations.
     */
    public settings = new AudioClientSettings();

    /**
     * @internal
     * Restricted to internal use because CoreInterop is an internal implementation detail.
     * Users should create clients via the Model.createAudioClient() factory method.
     */
    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Creates a LiveAudioTranscriptionClient for real-time audio streaming ASR.
     * @returns A LiveAudioTranscriptionClient instance.
     */
    public createLiveTranscriptionClient(): LiveAudioTranscriptionClient {
        return new LiveAudioTranscriptionClient(this.modelId, this.coreInterop);
    }

    /**
     * Validates that the audio file path is a non-empty string.
     * @internal
     */
    private validateAudioFilePath(audioFilePath: string): void {
        if (typeof audioFilePath !== 'string' || audioFilePath.trim() === '') {
            throw new Error('Audio file path must be a non-empty string.');
        }
    }

    /**
     * Transcribes audio into the input language.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @returns The transcription result.
     * @throws Error - If audioFilePath is invalid or transcription fails.
     */
    public async transcribe(audioFilePath: string): Promise<any> {
        this.validateAudioFilePath(audioFilePath);
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };

        try {
            const response = this.coreInterop.executeCommand("audio_transcribe", { Params: { OpenAICreateRequest: JSON.stringify(request) } });
            return JSON.parse(response);
        } catch (error) {
            throw new Error(`Audio transcription failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`, { cause: error });
        }
    }

    /**
     * Transcribes audio into the input language using streaming, returning an async iterable of chunks.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @returns An async iterable that yields parsed streaming transcription chunks.
     * @throws Error - If audioFilePath is invalid, or streaming fails.
     *
     * @example
     * ```typescript
     * for await (const chunk of audioClient.transcribeStreaming('recording.wav')) {
     *     process.stdout.write(chunk.text);
     * }
     * ```
     */
    public transcribeStreaming(audioFilePath: string): AsyncIterable<any> {
        this.validateAudioFilePath(audioFilePath);

        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };

        // Capture instance properties to local variables because `this` is not
        // accessible inside the [Symbol.asyncIterator]() method below — it's a
        // regular method on the returned object literal, not on the AudioClient.
        const coreInterop = this.coreInterop;
        const modelId = this.modelId;

        // Return an AsyncIterable object. The [Symbol.asyncIterator]() factory
        // is called once when the consumer starts a `for await` loop, and it
        // returns the AsyncIterator (with next() / return() methods).
        return {
            [Symbol.asyncIterator](): AsyncIterator<any> {
                // Buffer for chunks received from the native callback.
                // Uses a head index for O(1) dequeue instead of Array.shift() which is O(n).
                // JavaScript's single-threaded event loop ensures no race conditions
                // between the callback pushing chunks and next() consuming them.
                const chunks: any[] = [];
                let head = 0;
                let done = false;
                let cancelled = false;
                let error: Error | null = null;
                let resolve: (() => void) | null = null;
                let nextInFlight = false;

                const streamingPromise = coreInterop.executeCommandStreaming(
                    "audio_transcribe",
                    { Params: { OpenAICreateRequest: JSON.stringify(request) } },
                    (chunkStr: string) => {
                        if (cancelled || error) return;
                        if (chunkStr) {
                            try {
                                const chunk = JSON.parse(chunkStr);
                                chunks.push(chunk);
                            } catch (e) {
                                if (!error) {
                                    error = new Error(
                                        `Failed to parse streaming chunk: ${e instanceof Error ? e.message : String(e)}`,
                                        { cause: e }
                                    );
                                }
                            }
                        }
                        // Wake up any waiting next() call
                        if (resolve) {
                            const r = resolve;
                            resolve = null;
                            r();
                        }
                    }
                // When the native stream completes, mark done and wake up any
                // pending next() call so it can see that iteration has ended.
                ).then(() => {
                    done = true;
                    if (resolve) {
                        const r = resolve;
                        resolve = null;
                        r(); // resolve the pending next() promise
                    }
                }).catch((err) => {
                    if (!error) {
                        const underlyingError = err instanceof Error ? err : new Error(String(err));
                        error = new Error(
                            `Streaming audio transcription failed for model '${modelId}': ${underlyingError.message}`,
                            { cause: underlyingError }
                        );
                    }
                    done = true;
                    if (resolve) {
                        const r = resolve;
                        resolve = null;
                        r();
                    }
                });

                // Return the AsyncIterator object consumed by `for await`.
                // next() yields buffered chunks one at a time; return() is
                // called automatically when the consumer breaks out early.
                return {
                    async next(): Promise<IteratorResult<any>> {
                        if (nextInFlight) {
                            throw new Error('next() called concurrently on streaming iterator; await each call before invoking next().');
                        }
                        nextInFlight = true;
                        try {
                            while (true) {
                                if (head < chunks.length) {
                                    const value = chunks[head];
                                    chunks[head] = undefined; // allow GC
                                    head++;
                                    // Compact the array when all buffered chunks have been consumed
                                    if (head === chunks.length) {
                                        chunks.length = 0;
                                        head = 0;
                                    }
                                    return { value, done: false };
                                }
                                if (error) {
                                    throw error;
                                }
                                if (done || cancelled) {
                                    return { value: undefined, done: true };
                                }
                                // Wait for the next chunk or completion
                                await new Promise<void>((r) => { resolve = r; });
                            }
                        } finally {
                            nextInFlight = false;
                        }
                    },
                    async return(): Promise<IteratorResult<any>> {
                        // Mark cancelled so the callback stops buffering.
                        // Note: the underlying native stream cannot be cancelled
                        // (CoreInterop.executeCommandStreaming has no abort support),
                        // so the koffi callback may still fire but will no-op due
                        // to the cancelled guard above.
                        cancelled = true;
                        chunks.length = 0;
                        head = 0;
                        if (resolve) {
                            const r = resolve;
                            resolve = null;
                            r();
                        }
                        return { value: undefined, done: true };
                    }
                };
            }
        };
    }
}
