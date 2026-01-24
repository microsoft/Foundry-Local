import { CoreInterop } from '../detail/coreInterop.js';

/**
 * Client for performing audio operations (transcription, translation) with a loaded model.
 * Follows the OpenAI Audio API structure.
 */
export class AudioClient {
    private modelId: string;
    private coreInterop: CoreInterop;

    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Transcribes audio into the input language.
     * @param audioFilePath - Path to the audio file to transcribe.
     * @returns The transcription result.
     */
    public async transcribe(audioFilePath: string, language: string | null = null, temperature: number = 0.0): Promise<any> {
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            Language: language,
            Temperature: temperature
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
    public async transcribeStreaming(audioFilePath: string, callback: (chunk: any) => void, language: string | null = null, temperature: number = 0.0): Promise<void> {
        const request = {
            Model: this.modelId,
            FileName: audioFilePath,
            Language: language,
            Temperature: temperature
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
