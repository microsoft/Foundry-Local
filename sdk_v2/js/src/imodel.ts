import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';

export interface IModel {
    get id(): string;
    get alias(): string;
    get isCached(): boolean;
    isLoaded(): Promise<boolean>;

    download(progressCallback?: (progress: number) => void): void;
    get path(): string;
    load(): Promise<void>;
    removeFromCache(): void;
    unload(): Promise<void>;

    createChatClient(): ChatClient;
    createAudioClient(): AudioClient;
}
