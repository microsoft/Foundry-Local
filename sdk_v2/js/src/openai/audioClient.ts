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
     * Transcribes audio into the input language using streaming.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @param callback - A callback function that receives each chunk of the streaming response.
     * @returns A promise that resolves when the stream is complete.
     * @throws Error - If audioFilePath or callback are invalid, or streaming fails.
     */
    public async transcribeStreaming(audioFilePath: string, callback: (chunk: any) => void): Promise<void> {
        this.validateAudioFilePath(audioFilePath);
        if (!callback || typeof callback !== 'function') {
            throw new Error('Callback must be a valid function.');
        }
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };
        
        let error: Error | null = null;

        try {
            await this.coreInterop.executeCommandStreaming(
                "audio_transcribe", 
                { Params: { OpenAICreateRequest: JSON.stringify(request) } },
                (chunkStr: string) => {
                    // Skip processing if we already encountered an error
                    if (error) {
                        return;
                    }
                    
                    if (chunkStr) {
                        let chunk: any;
                        try {
                            chunk = JSON.parse(chunkStr);
                        } catch (e) {
                            // Don't throw from callback - store first error and skip further processing
                            // to avoid unhandled exception in native callback context
                            error = new Error(`Failed to parse streaming chunk: ${e instanceof Error ? e.message : String(e)}`, { cause: e });
                            return;
                        }

                        try {
                            callback(chunk);
                        } catch (e) {
                            // Don't throw from callback - store first error and stop processing
                            error = new Error(`User callback threw an error: ${e instanceof Error ? e.message : String(e)}`, { cause: e });
                        }
                    }
                }
            );

            // If we encountered an error during streaming, reject now
            if (error) {
                throw error;
            }
        } catch (err) {
            // Don't double-wrap the first error we collected during streaming
            if (error && err === error) {
                throw err;
            }
            throw new Error(`Streaming audio transcription failed for model '${this.modelId}': ${err instanceof Error ? err.message : String(err)}`, { cause: err });
        }
    }
}
