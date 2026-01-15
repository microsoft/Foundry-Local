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

    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Performs a synchronous chat completion.
     * @param messages - An array of message objects (e.g., { role: 'user', content: 'Hello' }).
     * @returns The chat completion response object.
     */
    public async completeChat(messages: any[]): Promise<any> {
        const request = {
            model: this.modelId,
            messages,
            // stream is undefined (false) by default
            ...this.settings._serialize()
        };

        const response = this.coreInterop.executeCommand("chat_completions", { Params: { OpenAICreateRequest: JSON.stringify(request) } });
        return JSON.parse(response);
    }

    /**
     * Performs a streaming chat completion.
     * @param messages - An array of message objects.
     * @param callback - A callback function that receives each chunk of the streaming response.
     * @returns A promise that resolves when the stream is complete.
     */
    public async completeStreamingChat(messages: any[], callback: (chunk: any) => void): Promise<void> {
        const request = {
            model: this.modelId,
            messages,
            stream: true,
            ...this.settings._serialize()
        };
        
        await this.coreInterop.executeCommandStreaming(
            "chat_completions", 
            { Params: { OpenAICreateRequest: JSON.stringify(request) } },
            (chunkStr: string) => {
                if (chunkStr) {
                    try {
                        const chunk = JSON.parse(chunkStr);
                        callback(chunk);
                    } catch (e) {
                        throw new Error(`Failed completeStreamingChat: ${e}`);
                    }
                }
            }
        );
    }
}

