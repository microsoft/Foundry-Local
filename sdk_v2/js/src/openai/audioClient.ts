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
     * Transcribes audio into the input language.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @returns The transcription result.
     * @throws Error - If audioFilePath is invalid or transcription fails.
     */
    public async transcribe(audioFilePath: string): Promise<any> {
        if (typeof audioFilePath !== 'string' || audioFilePath.trim() === '') {
            throw new Error('Audio file path must be a non-empty string.');
        }
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
        if (typeof audioFilePath !== 'string' || audioFilePath.trim() === '') {
            throw new Error('Audio file path must be a non-empty string.');
        }
        if (!callback || typeof callback !== 'function') {
            throw new Error('Callback must be a valid function.');
        }
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };
        
        let parseError: Error | null = null;

        try {
            await this.coreInterop.executeCommandStreaming(
                "audio_transcribe", 
                { Params: { OpenAICreateRequest: JSON.stringify(request) } },
                (chunkStr: string) => {
                    if (chunkStr) {
                        try {
                            const chunk = JSON.parse(chunkStr);
                            callback(chunk);
                        } catch (e) {
                            // Don't throw from callback - store error and handle after streaming completes
                            // to avoid unhandled exception in native callback context
                            parseError = new Error(`Failed to parse streaming chunk: ${e instanceof Error ? e.message : String(e)}`);
                            console.error(`[AudioClient] ${parseError.message}`);
                        }
                    }
                }
            );

            // If we encountered parse errors during streaming, reject now
            if (parseError) {
                throw parseError;
            }
        } catch (error) {
            throw new Error(`Streaming audio transcription failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`, { cause: error });
        }
    }
}
