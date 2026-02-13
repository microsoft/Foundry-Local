import { CoreInterop } from '../detail/coreInterop.js';

export class ChatClientSettings {
    frequencyPenalty?: number;
    maxTokens?: number;
    n?: number;
    temperature?: number;
    presencePenalty?: number;
    randomSeed?: number;
    topK?: number;
    topP?: number;

    /**
     * Serializes the settings into an OpenAI-compatible request object.
     * @internal
     */
    _serialize() {
        // Standard OpenAI properties
        const result: any = {
            frequency_penalty: this.frequencyPenalty,
            max_tokens: this.maxTokens,
            n: this.n,
            presence_penalty: this.presencePenalty,
            temperature: this.temperature,
            top_p: this.topP,
        };

        // Foundry specific metadata properties
        const metadata: Record<string, string> = {};
        if (this.topK !== undefined) {
          metadata["top_k"] = this.topK.toString();
        }
        if (this.randomSeed !== undefined) {
            metadata["random_seed"] = this.randomSeed.toString();
        }
        
        if (Object.keys(metadata).length > 0) {
            result.metadata = metadata;
        }

        // Filter out undefined properties
        return Object.fromEntries(Object.entries(result).filter(([_, v]) => v !== undefined));
    }
}

/**
 * Client for performing chat completions with a loaded model.
 * Follows the OpenAI Chat Completion API structure.
 */
export class ChatClient {
    private modelId: string;
    private coreInterop: CoreInterop;

    /**
     * Configuration settings for chat completions.
     */
    public settings = new ChatClientSettings();

    /**
     * @internal
     * Restricted to internal use because CoreInterop is an internal implementation detail.
     * Users should create clients via the Model.createChatClient() factory method.
     */
    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Performs a synchronous chat completion.
     * @param messages - An array of message objects (e.g., { role: 'user', content: 'Hello' }).
     * @returns The chat completion response object.
     * @throws Error - If messages are invalid or completion fails.
     */
    public async completeChat(messages: any[]): Promise<any> {
        if (!messages || !Array.isArray(messages) || messages.length === 0) {
            throw new Error('Messages array cannot be null, undefined, or empty.');
        }
        for (const msg of messages) {
            if (!msg.role || !msg.content) {
                throw new Error('Each message must have both "role" and "content" properties.');
            }
        }
        const request = {
            model: this.modelId,
            messages,
            // stream is undefined (false) by default
            ...this.settings._serialize()
        };

        try {
            const response = this.coreInterop.executeCommand("chat_completions", { Params: { OpenAICreateRequest: JSON.stringify(request) } });
            return JSON.parse(response);
        } catch (error) {
            throw new Error(`Chat completion failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`);
        }
    }

    /**
     * Performs a streaming chat completion.
     * @param messages - An array of message objects.
     * @param callback - A callback function that receives each chunk of the streaming response.
     * @returns A promise that resolves when the stream is complete.
     * @throws Error - If messages or callback are invalid, or streaming fails.
     */
    public async completeStreamingChat(messages: any[], callback: (chunk: any) => void): Promise<void> {
        if (!messages || !Array.isArray(messages) || messages.length === 0) {
            throw new Error('Messages array cannot be null, undefined, or empty.');
        }
        if (!callback || typeof callback !== 'function') {
            throw new Error('Callback must be a valid function.');
        }
        for (const msg of messages) {
            if (!msg.role || !msg.content) {
                throw new Error('Each message must have both "role" and "content" properties.');
            }
        }
        const request = {
            model: this.modelId,
            messages,
            stream: true,
            ...this.settings._serialize()
        };
        
        try {
            await this.coreInterop.executeCommandStreaming(
                "chat_completions", 
                { Params: { OpenAICreateRequest: JSON.stringify(request) } },
                (chunkStr: string) => {
                    if (chunkStr) {
                        try {
                            const chunk = JSON.parse(chunkStr);
                            callback(chunk);
                        } catch (e) {
                            throw new Error(`Failed to parse streaming chunk: ${e}`);
                        }
                    }
                }
            );
        } catch (error) {
            throw new Error(`Streaming chat completion failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`);
        }
    }
}

