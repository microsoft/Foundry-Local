import { CoreInterop } from '../detail/coreInterop.js';

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

        const coreInterop = this.coreInterop;
        const modelId = this.modelId;

        return {
            [Symbol.asyncIterator](): AsyncIterator<any> {
                // Buffer for chunks received from the native callback.
                // JavaScript's single-threaded event loop ensures no race conditions
                // between the callback pushing chunks and next() consuming them.
                const chunks: any[] = [];
                let done = false;
                let cancelled = false;
                let error: Error | null = null;
                let resolve: (() => void) | null = null;

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
                ).then(() => {
                    done = true;
                    if (resolve) {
                        const r = resolve;
                        resolve = null;
                        r();
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

                return {
                    async next(): Promise<IteratorResult<any>> {
                        while (true) {
                            if (chunks.length > 0) {
                                return { value: chunks.shift()!, done: false };
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
                    },
                    async return(): Promise<IteratorResult<any>> {
                        cancelled = true;
                        chunks.length = 0;
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
