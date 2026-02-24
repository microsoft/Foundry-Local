import { CoreInterop } from '../detail/coreInterop.js';
import { ResponseFormat, ToolChoice } from '../types.js';

export class ChatClientSettings {
    frequencyPenalty?: number;
    maxTokens?: number;
    n?: number;
    temperature?: number;
    presencePenalty?: number;
    randomSeed?: number;
    topK?: number;
    topP?: number;
    responseFormat?: ResponseFormat;
    toolChoice?: ToolChoice;

    /**
     * Serializes the settings into an OpenAI-compatible request object.
     * @internal
     */
    _serialize() {
        // Run internal validations
        this.validateResponseFormat(this.responseFormat);
        this.validateToolChoice(this.toolChoice);

        // Helper function to filter out undefined properties from objects
        const filterUndefined = (obj: any): any => {
            return Object.fromEntries(Object.entries(obj).filter(([_, v]) => v !== undefined));
        };

        // Standard OpenAI properties
        const result: any = {
            frequency_penalty: this.frequencyPenalty,
            max_tokens: this.maxTokens,
            n: this.n,
            presence_penalty: this.presencePenalty,
            temperature: this.temperature,
            top_p: this.topP,
            response_format: this.responseFormat ? filterUndefined(this.responseFormat) : undefined,
            tool_choice: this.toolChoice ? filterUndefined(this.toolChoice) : undefined
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
        return filterUndefined(result);
    }

    /**
     * Validates that the provided ResponseFormat object is well-formed.
     * @internal
     * @param format
     */
    private validateResponseFormat(format?: ResponseFormat): void {
        if (!format) return;

        const validTypes = ['text', 'json_object', 'json_schema', 'lark_grammar'];
        if (!validTypes.includes(format.type)) {
            throw new Error(`ResponseFormat type must be one of: ${validTypes.join(', ')}`);
        }

        const validGrammarTypes = ['json_schema', 'lark_grammar'];
        if (validGrammarTypes.includes(format.type)) {
            if (format.type === 'json_schema' && (typeof format.jsonSchema !== 'string' || format.jsonSchema.trim() === '')) {
                throw new Error('ResponseFormat with type "json_schema" must have a valid jsonSchema string.');
            }
            if (format.type === 'lark_grammar' && (typeof format.larkGrammar !== 'string' || format.larkGrammar.trim() === '')) {
                throw new Error('ResponseFormat with type "lark_grammar" must have a valid larkGrammar string.');
            }
        }
        else if (format.jsonSchema || format.larkGrammar) {
            throw new Error(`ResponseFormat with type "${format.type}" should not have jsonSchema or larkGrammar properties.`);
        }
    }

    /**
     * Validates that the provided ToolChoice object is well-formed.
     * @internal
     * @param choice
     */
    private validateToolChoice(choice?: ToolChoice): void {
        if (!choice) return;

        const validTypes = ['none', 'auto', 'required', 'function'];
        if (!validTypes.includes(choice.type)) {
            throw new Error(`ToolChoice type must be one of: ${validTypes.join(', ')}`);
        }

        if (choice.type === 'function' && (typeof choice.name !== 'string' || choice.name.trim() === '')) {
            throw new Error('ToolChoice with type "function" must have a valid name string.');
        }
        else if (choice.type !== 'function' && choice.name) {
            throw new Error(`ToolChoice with type "${choice.type}" should not have a name property.`);
        }
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
     * Validates that messages array is properly formed.
     * @internal
     */
    private validateMessages(messages: any[]): void {
        if (!messages || !Array.isArray(messages) || messages.length === 0) {
            throw new Error('Messages array cannot be null, undefined, or empty.');
        }
        for (const msg of messages) {
            if (!msg || typeof msg !== 'object' || Array.isArray(msg)) {
                throw new Error('Each message must be a non-null object with both "role" and "content" properties.');
            }
            if (typeof msg.role !== 'string' || msg.role.trim() === '') {
                throw new Error('Each message must have a "role" property that is a non-empty string.');
            }
            if (typeof msg.content !== 'string' || msg.content.trim() === '') {
                throw new Error('Each message must have a "content" property that is a non-empty string.');
            }
        }
    }

    /**
     * Validates that tools array is properly formed.
     * @internal
     */
    private validateTools(tools: any[]): void {
        if (!tools || !Array.isArray(tools)) {
            throw new Error('Tools must be an array if provided.');
        }
        for (const tool of tools) {
            if (!tool || typeof tool !== 'object' || Array.isArray(tool)) {
                throw new Error('Each tool must be a non-null object with "name" and "description" properties.');
            }
            if (typeof tool.type !== 'string' || tool.type.trim() === '') {
                throw new Error('Each tool must have a "type" property that is a non-empty string.');
            }
            if (typeof tool.function !== 'object' || tool.function.description.trim() === '') {
                throw new Error('Each tool must have a "function" property that is a non-empty object.');
            }
            if (typeof tool.function.name !== 'string' || tool.function.name.trim() === '') {
                throw new Error('Each tool\'s function must have a "name" property that is a non-empty string.');
            }
        }
    }

    /**
     * Performs a synchronous chat completion.
     * @param messages - An array of message objects (e.g., { role: 'user', content: 'Hello' }).
     * @param tools - An array of tool objects (e.g. { type: 'function', function: { name: 'get_apps', description: 'Returns a list of apps available on the system' } }).
     * @returns The chat completion response object.
     * @throws Error - If messages or tools are invalid or completion fails.
     */
    public async completeChat(messages: any[], tools: any[]): Promise<any> {
        this.validateMessages(messages);
        this.validateTools(tools);
        const request = {
            model: this.modelId,
            messages,
            tools,
            // stream is undefined (false) by default
            ...this.settings._serialize()
        };

        try {
            const response = this.coreInterop.executeCommand("chat_completions", { Params: { OpenAICreateRequest: JSON.stringify(request) } });
            return JSON.parse(response);
        } catch (error) {
            throw new Error(`Chat completion failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`, { cause: error });
        }
    }

    /**
     * Performs a streaming chat completion.
     * @param messages - An array of message objects.
     * @param tools - An array of tool objects.
     * @param callback - A callback function that receives each chunk of the streaming response.
     * @returns A promise that resolves when the stream is complete.
     * @throws Error - If messages, tools, or callback are invalid, or streaming fails.
     */
    public async completeStreamingChat(messages: any[], tools: any[], callback: (chunk: any) => void): Promise<void> {
        this.validateMessages(messages);
        this.validateTools(tools);
        if (!callback || typeof callback !== 'function') {
            throw new Error('Callback must be a valid function.');
        }
        const request = {
            model: this.modelId,
            messages,
            tools,
            stream: true,
            ...this.settings._serialize()
        };
        
        let error: Error | null = null;

        try {
            await this.coreInterop.executeCommandStreaming(
                "chat_completions", 
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
                            // Don't throw from callback - store first error and stop processing
                            error = new Error(`Failed to parse streaming chunk: ${e instanceof Error ? e.message : String(e)}`, { cause: e });
                            return;
                        }

                        try {
                            callback(chunk);
                        } catch (e) {
                            // Don't throw from callback - store first error and stop processing
                            error = new Error(`User callback threw an error: ${e instanceof Error ? e.message : String(e)}`, { cause: e });
                            return;
                        }
                    }
                }
            );

            // If we encountered an error during streaming, reject now
            if (error) {
                throw error;
            }
        } catch (err) {
            const underlyingError = err instanceof Error ? err : new Error(String(err));
            throw new Error(
                `Streaming chat completion failed for model '${this.modelId}': ${underlyingError.message}`,
                { cause: underlyingError }
            );
        }
    }
}
