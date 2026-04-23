import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';
import { EmbeddingClient } from './openai/embeddingClient.js';
import { LiveAudioTranscriptionSession } from './openai/liveAudioTranscriptionClient.js';
import { ResponsesClient } from './openai/responsesClient.js';
import { ModelInfo } from './types.js';

export interface IModel {
    get id(): string;
    get alias(): string;
    get info(): ModelInfo;
    get isCached(): boolean;
    isLoaded(): Promise<boolean>;

    get contextLength(): number | null;
    get inputModalities(): string | null;
    get outputModalities(): string | null;
    get capabilities(): string | null;
    get supportsToolCalling(): boolean | null;

    download(progressCallback?: (progress: number) => void): Promise<void>;
    get path(): string;
    load(): Promise<void>;
    removeFromCache(): void;
    unload(): Promise<void>;

    createChatClient(): ChatClient;
    createAudioClient(): AudioClient;
    createEmbeddingClient(): EmbeddingClient;

    /**
     * Creates a LiveAudioTranscriptionSession for real-time audio streaming ASR.
     * The model must be loaded before calling this method.
     * @returns A LiveAudioTranscriptionSession instance.
     */
    createLiveTranscriptionSession(): LiveAudioTranscriptionSession;
    /**
     * Creates a ResponsesClient for interacting with the model via the Responses API.
     * Unlike createChatClient/createAudioClient (which use FFI), the Responses API
     * is HTTP-based, so the web service base URL must be provided.
     * @param baseUrl - The base URL of the Foundry Local web service.
     */
    createResponsesClient(baseUrl: string): ResponsesClient;

    /**
     * Variants of the model that are available. Variants of the model are optimized for different devices.
     */
    get variants(): IModel[];

    /**
     * Select a model variant from variants to use for IModel operations.
     * An IModel from `variants` can also be used directly.
     * @param variant - Model variant to select. Must be one of the variants in `variants`.
     * @throws Error if variant is not valid for this model.
     */
    selectVariant(variant: IModel): void;
}
