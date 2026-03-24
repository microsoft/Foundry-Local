import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';
import { LiveAudioTranscriptionClient } from './openai/liveAudioTranscriptionClient.js';
import { ResponsesClient } from './openai/responsesClient.js';

export interface IModel {
    get id(): string;
    get alias(): string;
    get isCached(): boolean;
    isLoaded(): Promise<boolean>;

    download(progressCallback?: (progress: number) => void): Promise<void>;
    get path(): string;
    load(): Promise<void>;
    removeFromCache(): void;
    unload(): Promise<void>;

    createChatClient(): ChatClient;
    createAudioClient(): AudioClient;

    /**
     * Creates a LiveAudioTranscriptionClient for real-time audio streaming ASR.
     * The model must be loaded before calling this method.
     * @returns A LiveAudioTranscriptionClient instance.
     */
    createLiveTranscriptionClient(): LiveAudioTranscriptionClient;
    /**
     * Creates a LiveAudioTranscriptionClient for real-time audio streaming ASR.
     * @returns A LiveAudioTranscriptionClient instance.
     */
    createLiveTranscriptionClient(): LiveAudioTranscriptionClient;
    /**
     * Creates a ResponsesClient for interacting with the model via the Responses API.
     * Unlike createChatClient/createAudioClient (which use FFI), the Responses API
     * is HTTP-based, so the web service base URL must be provided.
     * @param baseUrl - The base URL of the Foundry Local web service.
     */
    createResponsesClient(baseUrl: string): ResponsesClient;
}
