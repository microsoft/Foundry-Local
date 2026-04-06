import { CoreInterop } from '../detail/coreInterop.js';
import { ResponseFormat, ToolChoice } from '../types.js';

export class ChatClientSettings {
    frequencyPenalty?: number;
    maxTokens?: number;
    maxCompletionTokens?: number;
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
            max_completion_tokens: this.maxCompletionTokens,
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
    private validateTools(tools?: any[]): void {
        if (!tools) return; // tools are optional

        if (!Array.isArray(tools)) {
            throw new Error('Tools must be an array if provided.');
        }

        for (const tool of tools) {
            if (!tool || typeof tool !== 'object' || Array.isArray(tool)) {
                throw new Error('Each tool must be a non-null object with a valid "type" and "function" definition.');
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
    public async completeChat(messages: any[]): Promise<any>;
    public async completeChat(messages: any[], tools: any[]): Promise<any>;
    public async completeChat(messages: any[], tools?: any[]): Promise<any> {
        this.validateMessages(messages);
        this.validateTools(tools);

        const request = {
            model: this.modelId,
            messages,
            ...(tools ? { tools } : {}),
            // stream is undefined (false) by default
            ...this.settings._serialize()
        };

        try {
            const response = this.coreInterop.executeCommand('chat_completions', {
                Params: { OpenAICreateRequest: JSON.stringify(request) }
            });
            return JSON.parse(response);
        } catch (error) {
            throw new Error(
                `Chat completion failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`,
                { cause: error }
            );
        }
    }

    /**
     * Performs a streaming chat completion, returning an async iterable of chunks.
     * @param messages - An array of message objects.
     * @param tools - An optional array of tool objects.
     * @returns An async iterable that yields parsed streaming response chunks.
     * @throws Error - If messages or tools are invalid, or streaming fails.
     *
     * @example
     * ```typescript
     * // Without tools:
     * for await (const chunk of chatClient.completeStreamingChat(messages)) {
     *     const content = chunk.choices?.[0]?.delta?.content;
     *     if (content) process.stdout.write(content);
     * }
     *
     * // With tools:
     * for await (const chunk of chatClient.completeStreamingChat(messages, tools)) {
     *     const content = chunk.choices?.[0]?.delta?.content;
     *     if (content) process.stdout.write(content);
     * }
     * ```
     */
    public completeStreamingChat(messages: any[]): AsyncIterable<any>;
    public completeStreamingChat(messages: any[], tools: any[]): AsyncIterable<any>;
    public completeStreamingChat(messages: any[], tools?: any[]): AsyncIterable<any> {
        this.validateMessages(messages);
        this.validateTools(tools);

        const request = {
            model: this.modelId,
            messages,
            ...(tools ? { tools } : {}),
            stream: true,
            ...this.settings._serialize()
        };

        // Capture instance properties to local variables because `this` is not
        // accessible inside the [Symbol.asyncIterator]() method below — it's a
        // regular method on the returned object literal, not on the ChatClient.
        const coreInterop = this.coreInterop;
        const modelId = this.modelId;

        // Return an AsyncIterable object. The [Symbol.asyncIterator]() factory
        // is called once when the consumer starts a `for await` loop, and it
        // returns the AsyncIterator (with next() / return() methods).
        return {
            [Symbol.asyncIterator](): AsyncIterator<any> {
                // Buffer for chunks received from the native callback.
                // Uses a head index for O(1) dequeue instead of Array.shift() which is O(n).
                // JavaScript's single-threaded event loop ensures no race conditions
                // between the callback pushing chunks and next() consuming them.
                const chunks: any[] = [];
                let head = 0;
                let done = false;
                let cancelled = false;
                let error: Error | null = null;
                let resolve: (() => void) | null = null;
                let nextInFlight = false;

                const streamingPromise = coreInterop.executeCommandStreaming(
                    'chat_completions',
                    { Params: { OpenAICreateRequest: JSON.stringify(request) } },
                    (chunkStr: string) => {
                        if (cancelled || error) return;
                        if (chunkStr) {
                            try {
                                const chunk = JSON.parse(chunkStr);
                                chunks.push(chunk);
                            } catch (e) {
                                if (!error) {
                                    error = new Error(
                                        `Failed to parse streaming chunk: ${e instanceof Error ? e.message : String(e)}`,
                                        { cause: e }
                                    );
                                }
                            }
                        }
                        // Wake up any waiting next() call
                        if (resolve) {
                            const r = resolve;
                            resolve = null;
                            r();
                        }
                    }
                // When the native stream completes, mark done and wake up any
                // pending next() call so it can see that iteration has ended.
                ).then(() => {
                    done = true;
                    if (resolve) {
                        const r = resolve;
                        resolve = null;
                        r(); // resolve the pending next() promise
                    }
                }).catch((err) => {
                    if (!error) {
                        const underlyingError = err instanceof Error ? err : new Error(String(err));
                        error = new Error(
                            `Streaming chat completion failed for model '${modelId}': ${underlyingError.message}`,
                            { cause: underlyingError }
                        );
                    }
                    done = true;
                    if (resolve) {
                        const r = resolve;
                        resolve = null;
                        r();
                    }
                });

                // Return the AsyncIterator object consumed by `for await`.
                // next() yields buffered chunks one at a time; return() is
                // called automatically when the consumer breaks out early.
                return {
                    async next(): Promise<IteratorResult<any>> {
                        if (nextInFlight) {
                            throw new Error('next() called concurrently on streaming iterator; await each call before invoking next().');
                        }
                        nextInFlight = true;
                        try {
                            while (true) {
                                if (head < chunks.length) {
                                    const value = chunks[head];
                                    chunks[head] = undefined; // allow GC
                                    head++;
                                    // Compact the array when all buffered chunks have been consumed
                                    if (head === chunks.length) {
                                        chunks.length = 0;
                                        head = 0;
                                    }
                                    return { value, done: false };
                                }
                                if (error) {
                                    throw error;
                                }
                                if (done || cancelled) {
                                    return { value: undefined, done: true };
                                }
                                // Wait for the next chunk or completion
                                await new Promise<void>((r) => { resolve = r; });
                            }
                        } finally {
                            nextInFlight = false;
                        }
                    },
                    async return(): Promise<IteratorResult<any>> {
                        // Mark cancelled so the callback stops buffering.
                        // Note: the underlying native stream cannot be cancelled
                        // (CoreInterop.executeCommandStreaming has no abort support),
                        // so the koffi callback may still fire but will no-op due
                        // to the cancelled guard above.
                        cancelled = true;
                        chunks.length = 0;
                        head = 0;
                        if (resolve) {
                            const r = resolve;
                            resolve = null;
                            r();
                        }
                        return { value: undefined, done: true };
                    }
                };
            }
        };
    }
}
