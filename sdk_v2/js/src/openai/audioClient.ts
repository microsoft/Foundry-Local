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

    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Transcribes audio into the input language.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @returns The transcription result.
     */
    public async transcribe(audioFilePath: string): Promise<any> {
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };

        const response = this.coreInterop.executeCommand("audio_transcribe", { Params: { OpenAICreateRequest: JSON.stringify(request) } });
        return JSON.parse(response);
    }

    /**
     * Transcribes audio into the input language using streaming.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @param callback - A callback function that receives each chunk of the streaming response.
     * @returns A promise that resolves when the stream is complete.
     */
    public async transcribeStreaming(audioFilePath: string, callback: (chunk: any) => void): Promise<void> {
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            ...this.settings._serialize()
        };
        
        await this.coreInterop.executeCommandStreaming(
            "audio_transcribe", 
            { Params: { OpenAICreateRequest: JSON.stringify(request) } },
            (chunkStr: string) => {
                if (chunkStr) {
                    try {
                        const chunk = JSON.parse(chunkStr);
                        callback(chunk);
                    } catch (e) {
                        throw new Error(`Failed transcribeStreaming: ${e}`);
                    }
                }
            }
        );
    }
}
