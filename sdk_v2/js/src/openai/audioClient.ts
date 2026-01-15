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
     * @param audioFile - The audio file to transcribe.
     * @param options - Optional parameters for transcription.
     * @returns The transcription result.
     * @throws Error - Not implemented.
     */
    public async transcribe(audioFile: any, options?: any): Promise<any> {
        throw new Error("Synchronous audio transcription is not implemented.");
    }

    /**
     * Transcribes audio into the input language using streaming.
     * @param audioFile - The audio file to transcribe.
     * @param options - Optional parameters for transcription.
     * @returns The transcription result.
     * @throws Error - Not implemented.
     */
    public async transcribeStreaming(audioFile: any, options?: any): Promise<any> {
        throw new Error("Streaming audio transcription is not implemented yet.");
    }
}
