import {
    ResponseCreateParams,
    ResponseObject,
    ResponseToolChoice,
    TruncationStrategy,
    TextConfig,
    ReasoningConfig,
    FunctionToolDefinition,
    StreamingEvent,
    InputItemsListResponse,
    DeleteResponseResult,
    ListResponsesResult,
    ListResponsesOptions,
    ResponseInputItem,
    ResponseOutputItem,
    MessageItem,
    ContentPart,
    FunctionCallItem,
    FunctionCallOutputItem,
    InputImageContent,
    OutputTextContent,
} from '../types.js';
import { randomUUID } from 'crypto';

interface ResponsesCoreInterop {
    executeCommand(command: string, params?: any): string;
    executeCommandStreaming(command: string, params: any, callback: (chunk: string) => void): Promise<string>;
}

interface StoredFfiResponse {
    response: ResponseObject;
    input: ResponseInputItem[];
}

class ResponsesCallbackError extends Error { }

/**
 * Extracts the text content from an assistant message in a Response.
 * Equivalent to OpenAI Python SDK's `response.output_text`.
 *
 * @param response - The Response object.
 * @returns The concatenated text from the first assistant message, or an empty string.
 */
export function getOutputText(response: ResponseObject): string {
    for (const item of response.output) {
        if (item.type === 'message' && (item as MessageItem).role === 'assistant') {
            const content = (item as MessageItem).content;
            if (typeof content === 'string') return content;
            if (Array.isArray(content)) {
                return content
                    .filter((p: ContentPart) => 'text' in p)
                    .map((p: ContentPart) => (p as { text: string }).text)
                    .join('');
            }
        }
    }
    return '';
}

/**
 * Configuration settings for the Responses API client.
 * Properties use camelCase in JS and are serialized to snake_case for the API.
 */
export class ResponsesClientSettings {
    /** System-level instructions to guide the model. */
    instructions?: string;
    temperature?: number;
    topP?: number;
    maxOutputTokens?: number;
    frequencyPenalty?: number;
    presencePenalty?: number;
    toolChoice?: ResponseToolChoice;
    truncation?: TruncationStrategy;
    parallelToolCalls?: boolean;
    /**
     * Whether to store the response server-side so it can be retrieved via `get()`, `list()`,
     * `getInputItems()`, or referenced by `previous_response_id`. Defaults to `true` when not
     * explicitly set. Set to `false` to disable persistence for a given client.
     */
    store?: boolean;
    metadata?: Record<string, string>;
    reasoning?: ReasoningConfig;
    text?: TextConfig;
    seed?: number;

    /**
     * Serializes settings into an OpenAI Responses API-compatible request object.
     * @internal
     */
    _serialize(): Partial<ResponseCreateParams> {
        const filterUndefined = (obj: any): any =>
            Object.fromEntries(Object.entries(obj).filter(([_, v]) => v !== undefined));

        const result: Record<string, unknown> = {
            instructions: this.instructions,
            temperature: this.temperature,
            top_p: this.topP,
            max_output_tokens: this.maxOutputTokens,
            frequency_penalty: this.frequencyPenalty,
            presence_penalty: this.presencePenalty,
            tool_choice: this.toolChoice,
            truncation: this.truncation,
            parallel_tool_calls: this.parallelToolCalls,
            // Default store to true when not explicitly set
            store: this.store !== undefined ? this.store : true,
            metadata: this.metadata,
            reasoning: this.reasoning ? filterUndefined(this.reasoning) : undefined,
            text: this.text ? filterUndefined(this.text) : undefined,
            seed: this.seed,
        };

        // Filter out undefined properties
        return filterUndefined(result) as Partial<ResponseCreateParams>;
    }
}

/**
 * Client for the OpenAI Responses API.
 *
 * When created by FoundryLocalManager or IModel factories, create/createStreaming use
 * the native FFI path where possible and fall back to HTTP when the web service is
 * available. Stored-response operations (get/delete/cancel/input_items/list) use the
 * in-memory FFI store for FFI-created responses and HTTP for server-backed responses.
 *
 * Create via `FoundryLocalManager.createResponsesClient()` or
 * `model.createResponsesClient(baseUrl)`.
 *
 * @example
 * ```typescript
 * const manager = FoundryLocalManager.create({ appName: 'MyApp' });
 * manager.startWebService();
 * const client = manager.createResponsesClient('my-model-id');
 *
 * // Non-streaming
 * const response = await client.create('Hello, world!');
 * console.log(response.output);
 *
 * // Streaming
 * await client.createStreaming('Tell me a story', (event) => {
 *     if (event.type === 'response.output_text.delta') {
 *         process.stdout.write(event.delta);
 *     }
 * });
 * ```
 */
export class ResponsesClient {
    private baseUrl?: string;
    private modelId?: string;
    private coreInterop?: ResponsesCoreInterop;
    private readonly ffiStore = new Map<string, StoredFfiResponse>();

    /**
     * Configuration settings for responses.
     */
    public settings = new ResponsesClientSettings();

    /**
     * @param baseUrl - The base URL of the Foundry Local web service (e.g. "http://127.0.0.1:5273").
     * @param modelId - Optional default model ID. Can be overridden per-request via options.
     */
    constructor(baseUrl: string, modelId?: string) {
        this.initialize(baseUrl, modelId);
    }

    /**
     * @internal
     * Creates a hybrid client that can use FFI first and HTTP as a fallback.
     */
    public static createWithCoreInterop(
        baseUrl: string | undefined,
        modelId: string | undefined,
        coreInterop: ResponsesCoreInterop
    ): ResponsesClient {
        const client = new ResponsesClient(baseUrl?.trim() ? baseUrl : 'http://127.0.0.1', modelId);
        client.coreInterop = coreInterop;
        if (baseUrl === undefined || baseUrl.trim() === '') {
            client.baseUrl = undefined;
        }
        return client;
    }

    private initialize(baseUrl: string, modelId?: string): void {
        if (baseUrl === null || baseUrl === undefined || typeof baseUrl !== 'string') {
            throw new Error('baseUrl must be a non-empty string.');
        }
        if (baseUrl.trim() === '') {
            throw new Error('baseUrl must be a non-empty string.');
        }
        // Strip trailing slashes for consistent URL construction
        let url = baseUrl;
        while (url.endsWith('/')) {
            url = url.slice(0, -1);
        }
        this.baseUrl = url;
        this.modelId = modelId;
    }

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * Creates a model response (non-streaming).
     * @param input - A string prompt or array of input items.
     * @param options - Additional request parameters that override client settings.
     *   The `model` field is optional here if a default model was set in the constructor.
     * @returns The completed Response object. Check `response.status` and `response.error`
     *   even on success — the server returns HTTP 200 for model-level failures too.
     */
    public async create(
        input: string | ResponseInputItem[],
        options?: Partial<ResponseCreateParams>
    ): Promise<ResponseObject> {
        this.validateInput(input);
        if (options?.tools) {
            this.validateTools(options.tools);
        }

        const body = this.buildRequest(input, { ...options, stream: false });

        if (this.coreInterop) {
            try {
                const response = await this.createViaFfi(body);
                this.storeFfiResponseIfNeeded(response, body);
                return response;
            } catch (e) {
                if (!this.baseUrl) {
                    throw new Error(
                        `Responses FFI create failed and no HTTP fallback is available: ${e instanceof Error ? e.message : String(e)}`,
                        { cause: e }
                    );
                }
            }
        }

        return this.fetchJson<ResponseObject>(
            '/v1/responses',
            { method: 'POST', body: JSON.stringify(body) }
        );
    }

    /**
     * Creates a model response with streaming via Server-Sent Events.
     * @param input - A string prompt or array of input items.
     * @param callback - Called for each streaming event received.
     * @param options - Additional request parameters that override client settings.
     */
    public async createStreaming(
        input: string | ResponseInputItem[],
        callback: (event: StreamingEvent) => void,
        options?: Partial<ResponseCreateParams>
    ): Promise<void> {
        this.validateInput(input);
        if (options?.tools) {
            this.validateTools(options.tools);
        }
        if (!callback || typeof callback !== 'function') {
            throw new Error('Callback must be a valid function.');
        }

        const body = this.buildRequest(input, { ...options, stream: true });

        if (this.coreInterop) {
            try {
                const response = await this.createStreamingViaFfi(body, callback);
                this.storeFfiResponseIfNeeded(response, body);
                return;
            } catch (e) {
                if (e instanceof ResponsesCallbackError) {
                    throw e;
                }
                if (!this.baseUrl) {
                    throw new Error(
                        `Responses FFI streaming create failed and no HTTP fallback is available: ${e instanceof Error ? e.message : String(e)}`,
                        { cause: e }
                    );
                }
            }
        }

        const res = await this.doFetch('/v1/responses', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Accept': 'text/event-stream' },
            body: JSON.stringify(body),
        });

        if (!res.body) {
            throw new Error('Streaming response has no body.');
        }

        let error: Error | null = null;

        await this.parseSSEStream(res.body, (event: StreamingEvent) => {
            if (error) return;

            try {
                callback(event);
            } catch (e) {
                error = new Error(
                    `User callback threw an error: ${e instanceof Error ? e.message : String(e)}`,
                    { cause: e }
                );
            }
        });

        if (error) {
            throw error;
        }
    }

    /**
     * Retrieves a stored response by ID.
     * @param responseId - The ID of the response to retrieve.
     * @returns The Response object, or throws if not found.
     */
    public async get(responseId: string): Promise<ResponseObject> {
        this.validateId(responseId, 'responseId');
        const stored = this.ffiStore.get(responseId);
        if (stored) {
            return stored.response;
        }
        return this.fetchJson<ResponseObject>(
            `/v1/responses/${encodeURIComponent(responseId)}`,
            { method: 'GET' }
        );
    }

    /**
     * Deletes a stored response by ID.
     * @param responseId - The ID of the response to delete.
     * @returns The deletion result.
     */
    public async delete(responseId: string): Promise<DeleteResponseResult> {
        this.validateId(responseId, 'responseId');
        if (this.ffiStore.delete(responseId)) {
            return { id: responseId, object: 'response.deleted', deleted: true };
        }
        return this.fetchJson<DeleteResponseResult>(
            `/v1/responses/${encodeURIComponent(responseId)}`,
            { method: 'DELETE' }
        );
    }

    /**
     * Cancels an in-progress response.
     * @param responseId - The ID of the response to cancel.
     * @returns The cancelled Response object.
     */
    public async cancel(responseId: string): Promise<ResponseObject> {
        this.validateId(responseId, 'responseId');
        const stored = this.ffiStore.get(responseId);
        if (stored) {
            throw new Error('Cancellation is only supported for HTTP-backed Responses API operations.');
        }
        return this.fetchJson<ResponseObject>(
            `/v1/responses/${encodeURIComponent(responseId)}/cancel`,
            { method: 'POST' }
        );
    }

    /**
     * Retrieves input items for a stored response.
     * @param responseId - The ID of the response.
     * @returns The list of input items.
     */
    public async getInputItems(responseId: string): Promise<InputItemsListResponse> {
        this.validateId(responseId, 'responseId');
        const stored = this.ffiStore.get(responseId);
        if (stored) {
            return { object: 'list', data: stored.input };
        }
        return this.fetchJson<InputItemsListResponse>(
            `/v1/responses/${encodeURIComponent(responseId)}/input_items`,
            { method: 'GET' }
        );
    }

    /**
     * Lists stored responses.
     * @param options - Optional pagination parameters. The Foundry Local server supports
     *   `limit`, `order`, and `after`; it does not currently support `before`.
     * @returns The list of Response objects.
     */
    public async list(options?: ListResponsesOptions): Promise<ListResponsesResult> {
        const query = new URLSearchParams();
        if (options?.limit !== undefined) query.set('limit', String(options.limit));
        if (options?.order !== undefined) query.set('order', options.order);
        if (options?.after !== undefined) query.set('after', options.after);

        if (!this.baseUrl) {
            return this.listFfiResponses(options);
        }

        const queryString = query.toString();
        const path = queryString ? `/v1/responses?${queryString}` : '/v1/responses';
        const serverResult = await this.fetchJson<ListResponsesResult>(path, { method: 'GET' });
        const localResult = this.listFfiResponses(options);
        if (localResult.data.length === 0) {
            return serverResult;
        }

        const localIds = new Set(localResult.data.map((response) => response.id));
        const mergedData = [...localResult.data, ...serverResult.data.filter((response) => !localIds.has(response.id))];
        const order = options?.order ?? 'desc';
        mergedData.sort((a, b) =>
            order === 'asc' ? a.created_at - b.created_at : b.created_at - a.created_at
        );
        const limit = options?.limit ?? 20;
        const limitedData = mergedData.slice(0, limit);

        return {
            object: 'list',
            data: limitedData,
            first_id: limitedData[0]?.id ?? null,
            last_id: limitedData[limitedData.length - 1]?.id ?? null,
            has_more: Boolean(serverResult.has_more || localResult.has_more || mergedData.length > limitedData.length),
        };
    }

    // ========================================================================
    // Internal helpers
    // ========================================================================

    /**
     * Builds the full request body by merging input, settings, and per-call options.
     */
    private buildRequest(
        input: string | ResponseInputItem[],
        options?: Partial<ResponseCreateParams>
    ): ResponseCreateParams {
        const model = options?.model ?? this.modelId;
        if (!model || typeof model !== 'string' || model.trim() === '') {
            throw new Error(
                'Model must be specified either in the constructor, via createResponsesClient(modelId), or in options.model.'
            );
        }

        const serializedSettings = this.settings._serialize();

        // Merge order: model+input → settings defaults → per-call overrides
        return {
            model,
            input,
            ...serializedSettings,
            ...options,
        };
    }

    /**
     * Validates that input is a non-empty string or a non-empty array of items.
     */
    private validateInput(input: string | ResponseInputItem[]): void {
        if (input === null || input === undefined) {
            throw new Error('Input cannot be null or undefined.');
        }
        if (typeof input === 'string') {
            if (input.trim() === '') {
                throw new Error('Input string cannot be empty.');
            }
            return;
        }
        if (Array.isArray(input)) {
            if (input.length === 0) {
                throw new Error('Input items array cannot be empty.');
            }
            for (const item of input) {
                if (!item || typeof item !== 'object') {
                    throw new Error('Each input item must be a non-null object.');
                }
                if (typeof (item as any).type !== 'string' || (item as any).type.trim() === '') {
                    throw new Error('Each input item must have a "type" property that is a non-empty string.');
                }
            }
            return;
        }
        throw new Error('Input must be a string or an array of input items.');
    }

    /**
     * Validates that tools array is properly formed.
     * Follows the same pattern as ChatClient.validateTools.
     */
    private validateTools(tools: FunctionToolDefinition[]): void {
        if (!Array.isArray(tools)) {
            throw new Error('Tools must be an array if provided.');
        }
        for (const tool of tools) {
            if (!tool || typeof tool !== 'object' || Array.isArray(tool)) {
                throw new Error('Each tool must be a non-null object with a valid "type" and "name".');
            }
            if (tool.type !== 'function') {
                throw new Error('Each tool must have type "function".');
            }
            if (typeof tool.name !== 'string' || tool.name.trim() === '') {
                throw new Error('Each tool must have a "name" property that is a non-empty string.');
            }
        }
    }

    /**
     * Validates that a string ID parameter is non-empty and within length bounds.
     */
    private validateId(id: string, paramName: string): void {
        if (!id || typeof id !== 'string' || id.trim() === '') {
            throw new Error(`${paramName} must be a non-empty string.`);
        }
        if (id.length > 1024) {
            throw new Error(`${paramName} exceeds maximum length (1024).`);
        }
    }

    /**
     * Creates a response through the native chat_completions FFI command.
     */
    private async createViaFfi(body: ResponseCreateParams): Promise<ResponseObject> {
        if (!this.coreInterop) {
            throw new Error('Responses FFI transport is not available.');
        }

        const chatRequest = this.buildChatCompletionRequest(body, false);
        const raw = this.coreInterop.executeCommand('chat_completions', {
            Params: {
                OpenAICreateRequest: JSON.stringify(chatRequest),
            },
        });
        const chatResponse = this.parseFfiJson(raw, 'chat_completions');
        return this.mapChatCompletionToResponse(chatResponse, body);
    }

    /**
     * Creates a streaming response through the native chat_completions FFI command.
     */
    private async createStreamingViaFfi(
        body: ResponseCreateParams,
        callback: (event: StreamingEvent) => void
    ): Promise<ResponseObject> {
        if (!this.coreInterop) {
            throw new Error('Responses FFI transport is not available.');
        }
        if (body.tools && body.tools.length > 0) {
            throw new Error('Responses FFI streaming does not currently support tool calls.');
        }

        const responseId = this.createResponseId();
        const outputItemId = this.createItemId('msg');
        const chatRequest = this.buildChatCompletionRequest(body, true);
        const outputText: OutputTextContent = { type: 'output_text', text: '' };
        const messageItem: MessageItem = {
            id: outputItemId,
            type: 'message',
            role: 'assistant',
            content: [outputText],
            status: 'in_progress',
        };

        let sequence = 0;
        let contentPartStarted = false;
        let callbackError: ResponsesCallbackError | null = null;
        const response = this.createBaseResponse(body, responseId, [], 'in_progress');
        const emit = (event: any): void => {
            if (callbackError) return;
            try {
                callback({ ...event, sequence_number: sequence++ } as StreamingEvent);
            } catch (e) {
                callbackError = new ResponsesCallbackError(
                    `User callback threw an error: ${e instanceof Error ? e.message : String(e)}`
                );
                (callbackError as Error).cause = e;
            }
        };

        emit({ type: 'response.created', response });
        emit({ type: 'response.in_progress', response });

        const processChunk = (chunk: string): void => {
            if (callbackError) return;
            const parsed = this.parseStreamingFfiChunk(chunk);
            if (!parsed) return;

            const choices = Array.isArray(parsed.choices) ? parsed.choices : [];
            for (const choice of choices) {
                const delta = choice?.delta ?? choice?.Delta ?? {};
                const content = this.extractDeltaContent(delta);
                if (!content) continue;

                if (!contentPartStarted) {
                    emit({
                        type: 'response.output_item.added',
                        item_id: outputItemId,
                        output_index: 0,
                        item: messageItem,
                    });
                    emit({
                        type: 'response.content_part.added',
                        item_id: outputItemId,
                        output_index: 0,
                        content_index: 0,
                        part: outputText,
                    });
                    contentPartStarted = true;
                }

                outputText.text += content;
                emit({
                    type: 'response.output_text.delta',
                    item_id: outputItemId,
                    output_index: 0,
                    content_index: 0,
                    delta: content,
                });
            }
        };

        await this.coreInterop.executeCommandStreaming(
            'chat_completions',
            {
                Params: {
                    OpenAICreateRequest: JSON.stringify(chatRequest),
                },
            },
            processChunk
        );

        if (callbackError) {
            throw callbackError;
        }

        const finalMessage: MessageItem = {
            ...messageItem,
            content: [outputText],
            status: 'completed',
        };
        const completedResponse = this.createBaseResponse(
            body,
            responseId,
            contentPartStarted ? [finalMessage] : [],
            'completed'
        );
        completedResponse.completed_at = Math.floor(Date.now() / 1000);

        if (contentPartStarted) {
            emit({
                type: 'response.output_text.done',
                item_id: outputItemId,
                output_index: 0,
                content_index: 0,
                text: outputText.text,
            });
            emit({
                type: 'response.content_part.done',
                item_id: outputItemId,
                output_index: 0,
                content_index: 0,
                part: outputText,
            });
            emit({
                type: 'response.output_item.done',
                item_id: outputItemId,
                output_index: 0,
                item: finalMessage,
            });
        }
        emit({ type: 'response.completed', response: completedResponse });

        return completedResponse;
    }

    private buildChatCompletionRequest(body: ResponseCreateParams, stream: boolean): Record<string, unknown> {
        const request: Record<string, unknown> = {
            model: body.model,
            messages: this.convertResponseInputToChatMessages(body),
            stream,
        };

        if (body.temperature !== undefined) request.temperature = body.temperature;
        if (body.top_p !== undefined) request.top_p = body.top_p;
        if (body.max_output_tokens !== undefined) request.max_tokens = body.max_output_tokens;
        if (body.frequency_penalty !== undefined) request.frequency_penalty = body.frequency_penalty;
        if (body.presence_penalty !== undefined) request.presence_penalty = body.presence_penalty;
        if (body.seed !== undefined) request.seed = body.seed;
        if (body.metadata !== undefined) request.metadata = body.metadata;
        if (body.parallel_tool_calls !== undefined) request.parallel_tool_calls = body.parallel_tool_calls;
        if (body.tools !== undefined) request.tools = body.tools.map((tool) => this.convertResponseToolToChatTool(tool));
        if (body.tool_choice !== undefined) request.tool_choice = this.convertResponseToolChoiceToChatToolChoice(body.tool_choice);
        const responseFormat = this.convertTextConfigToChatResponseFormat(body.text);
        if (responseFormat !== undefined) request.response_format = responseFormat;

        return request;
    }

    private convertResponseInputToChatMessages(body: ResponseCreateParams): any[] {
        const messages: any[] = [];
        if (body.instructions) {
            messages.push({ role: 'system', content: body.instructions });
        }
        if (body.previous_response_id) {
            const previousResponse = this.ffiStore.get(body.previous_response_id);
            if (!previousResponse) {
                throw new Error(
                    `Responses FFI store does not contain previous_response_id: ${body.previous_response_id}`
                );
            }
            messages.push(...this.convertStoredFfiResponseToChatMessages(previousResponse));
        }

        const input = body.input;
        if (typeof input === 'string') {
            messages.push({ role: 'user', content: input });
            return messages;
        }

        if (!Array.isArray(input)) {
            throw new Error('Responses FFI create requires string input or an array of input items.');
        }

        for (const item of input) {
            if (item.type === 'message') {
                messages.push({
                    role: this.convertResponseRoleToChatRole(item.role),
                    content: this.convertContentToChatContent(item.content),
                });
            } else if (item.type === 'function_call') {
                messages.push(this.convertFunctionCallItemToChatMessage(item));
            } else if (item.type === 'function_call_output') {
                messages.push(this.convertFunctionCallOutputItemToChatMessage(item));
            } else if (item.type === 'item_reference') {
                const stored = this.ffiStore.get(item.id);
                if (!stored) {
                    throw new Error(`Responses FFI store does not contain referenced item: ${item.id}`);
                }
                messages.push(...this.convertResponseInputToChatMessages({ ...body, input: stored.input }));
            }
        }

        if (messages.length === 0) {
            throw new Error('Responses FFI create requires at least one message input item.');
        }
        return messages;
    }

    private convertStoredFfiResponseToChatMessages(stored: StoredFfiResponse): any[] {
        const messages = this.convertResponseInputToChatMessages({
            input: stored.input,
            model: stored.response.model,
        });
        for (const item of stored.response.output) {
            if (item.type === 'message') {
                messages.push({
                    role: this.convertResponseRoleToChatRole(item.role),
                    content: this.convertContentToChatContent(item.content),
                });
            } else if (item.type === 'function_call') {
                messages.push(this.convertFunctionCallItemToChatMessage(item));
            }
        }
        return messages;
    }

    private convertResponseRoleToChatRole(role: MessageItem['role']): string {
        if (role === 'developer') return 'system';
        return role;
    }

    private convertContentToChatContent(content: string | ContentPart[]): any {
        if (typeof content === 'string') {
            return content;
        }

        const parts: any[] = [];
        for (const part of content) {
            if (part.type === 'input_text') {
                parts.push({ type: 'text', text: part.text });
            } else if (part.type === 'output_text') {
                parts.push({ type: 'text', text: part.text });
            } else if (part.type === 'input_image') {
                parts.push({ type: 'image_url', image_url: this.convertInputImageToChatImageUrl(part) });
            } else if (part.type === 'refusal') {
                parts.push({ type: 'text', text: part.refusal });
            } else {
                throw new Error(`Responses FFI create does not support content part type: ${part.type}`);
            }
        }

        if (parts.length === 0) {
            return '';
        }
        if (parts.every((part) => part.type === 'text')) {
            return parts.map((part) => part.text).join('');
        }
        return parts;
    }

    private convertInputImageToChatImageUrl(part: InputImageContent): any {
        if (part.image_url) {
            return part.detail ? { url: part.image_url, detail: part.detail } : { url: part.image_url };
        }
        if (part.image_data) {
            if (!part.media_type) {
                throw new Error('Responses FFI create requires media_type when image_data is provided.');
            }
            const url = `data:${part.media_type};base64,${part.image_data}`;
            return part.detail ? { url, detail: part.detail } : { url };
        }
        throw new Error('Responses FFI create requires input_image to include image_url or image_data.');
    }

    private convertFunctionCallItemToChatMessage(item: FunctionCallItem): any {
        return {
            role: 'assistant',
            content: null,
            tool_calls: [{
                id: item.call_id,
                type: 'function',
                function: {
                    name: item.name,
                    arguments: item.arguments,
                },
            }],
        };
    }

    private convertFunctionCallOutputItemToChatMessage(item: FunctionCallOutputItem): any {
        return {
            role: 'tool',
            tool_call_id: item.call_id,
            content: this.convertContentPartOutputToText(item.output),
        };
    }

    private convertContentPartOutputToText(output: string | ContentPart[]): string {
        if (typeof output === 'string') {
            return output;
        }
        return output.map((part) => {
            if (part.type === 'input_text' || part.type === 'output_text') return part.text;
            if (part.type === 'refusal') return part.refusal;
            if (part.type === 'input_image') return part.image_url ?? '[image]';
            if (part.type === 'input_file') return part.file_url;
            return '';
        }).join('');
    }

    private convertResponseToolToChatTool(tool: FunctionToolDefinition): Record<string, unknown> {
        return {
            type: 'function',
            function: {
                name: tool.name,
                description: tool.description,
                parameters: tool.parameters,
                strict: tool.strict,
            },
        };
    }

    private convertResponseToolChoiceToChatToolChoice(toolChoice: ResponseToolChoice): unknown {
        if (typeof toolChoice === 'string') {
            return toolChoice;
        }
        if (toolChoice?.type === 'function') {
            return {
                type: 'function',
                function: { name: toolChoice.name },
            };
        }
        return toolChoice;
    }

    private convertTextConfigToChatResponseFormat(text?: TextConfig): unknown {
        const format = text?.format;
        if (!format) {
            return undefined;
        }
        if (format.type === 'json_schema') {
            return {
                type: 'json_schema',
                json_schema: {
                    name: format.name,
                    description: format.description,
                    schema: format.schema,
                    strict: format.strict,
                },
            };
        }
        if (format.type === 'json_object') {
            return { type: 'json_object' };
        }
        return { type: format.type };
    }

    private parseFfiJson(raw: string, command: string): any {
        try {
            const parsed = JSON.parse(raw);
            if (parsed?.Successful === false || parsed?.successful === false) {
                const error = parsed?.ErrorMessage ?? parsed?.errorMessage ?? parsed?.Error ?? parsed?.error ?? raw;
                throw new Error(String(error));
            }
            return parsed;
        } catch (e) {
            if (e instanceof SyntaxError) {
                throw new Error(`Failed to parse ${command} FFI response JSON: ${raw.substring(0, 200)}`, { cause: e });
            }
            throw e;
        }
    }

    private mapChatCompletionToResponse(chatResponse: any, body: ResponseCreateParams): ResponseObject {
        const responseId = this.createResponseId(chatResponse?.id);
        const createdAt = this.normalizeTimestamp(chatResponse?.created ?? chatResponse?.Created);
        const choices = Array.isArray(chatResponse?.choices) ? chatResponse.choices : [];
        const firstChoice = choices[0] ?? {};
        const message = firstChoice.message ?? firstChoice.Message ?? {};
        const output = this.convertChatMessageToResponseOutput(message);
        const response = this.createBaseResponse(body, responseId, output, 'completed');
        response.created_at = createdAt;
        response.completed_at = Math.floor(Date.now() / 1000);
        response.usage = this.mapUsage(chatResponse?.usage ?? chatResponse?.Usage);
        return response;
    }

    private convertChatMessageToResponseOutput(message: any): ResponseOutputItem[] {
        const toolCalls = message?.tool_calls ?? message?.ToolCalls;
        if (Array.isArray(toolCalls) && toolCalls.length > 0) {
            return toolCalls.map((toolCall: any) => ({
                type: 'function_call',
                id: this.createItemId('fc'),
                call_id: toolCall.id ?? toolCall.Id ?? this.createItemId('call'),
                name: toolCall.function?.name ?? toolCall.Function?.Name ?? '',
                arguments: toolCall.function?.arguments ?? toolCall.Function?.Arguments ?? '',
                status: 'completed',
            }));
        }

        const content = this.extractMessageContent(message);
        if (content === undefined) {
            return [];
        }

        const item: MessageItem = {
            type: 'message',
            id: this.createItemId('msg'),
            role: 'assistant',
            content: [{ type: 'output_text', text: content }],
            status: 'completed',
        };
        return [item];
    }

    private extractMessageContent(message: any): string | undefined {
        const content = message?.content ?? message?.Content;
        if (typeof content === 'string') {
            return content;
        }
        if (Array.isArray(content)) {
            return content.map((part) => {
                if (typeof part === 'string') return part;
                if (typeof part?.text === 'string') return part.text;
                if (typeof part?.Text === 'string') return part.Text;
                return '';
            }).join('');
        }
        return undefined;
    }

    private mapUsage(usage: any): ResponseObject['usage'] {
        if (!usage) {
            return null;
        }
        const inputTokens = usage.prompt_tokens ?? usage.PromptTokens ?? usage.input_tokens ?? 0;
        const outputTokens = usage.completion_tokens ?? usage.CompletionTokens ?? usage.output_tokens ?? 0;
        return {
            input_tokens: inputTokens,
            output_tokens: outputTokens,
            total_tokens: usage.total_tokens ?? usage.TotalTokens ?? inputTokens + outputTokens,
        };
    }

    private createBaseResponse(
        body: ResponseCreateParams,
        id: string,
        output: ResponseOutputItem[],
        status: ResponseObject['status']
    ): ResponseObject {
        return {
            id,
            object: 'response',
            created_at: Math.floor(Date.now() / 1000),
            completed_at: status === 'completed' ? Math.floor(Date.now() / 1000) : null,
            failed_at: null,
            cancelled_at: null,
            status,
            incomplete_details: null,
            model: body.model ?? this.modelId ?? '',
            previous_response_id: body.previous_response_id ?? null,
            instructions: body.instructions ?? null,
            output,
            error: null,
            tools: body.tools ?? [],
            tool_choice: body.tool_choice ?? 'auto',
            truncation: body.truncation ?? 'disabled',
            parallel_tool_calls: body.parallel_tool_calls ?? false,
            text: body.text ?? {},
            top_p: body.top_p ?? 1,
            temperature: body.temperature ?? 1,
            presence_penalty: body.presence_penalty ?? 0,
            frequency_penalty: body.frequency_penalty ?? 0,
            max_output_tokens: body.max_output_tokens ?? null,
            reasoning: body.reasoning ?? null,
            store: body.store ?? true,
            metadata: body.metadata ?? null,
            usage: null,
            user: body.user ?? null,
        };
    }

    private storeFfiResponseIfNeeded(response: ResponseObject, body: ResponseCreateParams): void {
        if (body.store === false) {
            return;
        }
        this.ffiStore.set(response.id, {
            response,
            input: this.normalizeResponseInputItems(body.input),
        });
    }

    private normalizeResponseInputItems(input: ResponseCreateParams['input']): ResponseInputItem[] {
        if (typeof input === 'string') {
            return [{
                type: 'message',
                role: 'user',
                content: input,
                status: 'completed',
            }];
        }
        return Array.isArray(input) ? input : [];
    }

    private listFfiResponses(options?: ListResponsesOptions): ListResponsesResult {
        const order = options?.order ?? 'desc';
        const limit = options?.limit ?? 20;
        let data = Array.from(this.ffiStore.values()).map((entry) => entry.response);
        data.sort((a, b) => order === 'asc' ? a.created_at - b.created_at : b.created_at - a.created_at);

        if (options?.after) {
            const afterIndex = data.findIndex((response) => response.id === options.after);
            data = afterIndex >= 0 ? data.slice(afterIndex + 1) : [];
        }

        const limitedData = data.slice(0, limit);
        return {
            object: 'list',
            data: limitedData,
            first_id: limitedData[0]?.id ?? null,
            last_id: limitedData[limitedData.length - 1]?.id ?? null,
            has_more: data.length > limitedData.length,
        };
    }

    private parseStreamingFfiChunk(chunk: string): any | undefined {
        const trimmed = chunk.trim();
        if (!trimmed || trimmed === '[DONE]') {
            return undefined;
        }
        const json = trimmed.startsWith('data: ') ? trimmed.slice(6).trim() : trimmed;
        if (!json || json === '[DONE]') {
            return undefined;
        }
        return JSON.parse(json);
    }

    private extractDeltaContent(delta: any): string {
        const content = delta?.content ?? delta?.Content;
        if (typeof content === 'string') {
            return content;
        }
        if (Array.isArray(content)) {
            return content.map((part) => {
                if (typeof part === 'string') return part;
                if (typeof part?.text === 'string') return part.text;
                if (typeof part?.Text === 'string') return part.Text;
                return '';
            }).join('');
        }
        return '';
    }

    private createResponseId(sourceId?: string): string {
        if (sourceId?.startsWith('resp_')) {
            return sourceId;
        }
        return `resp_${sourceId ?? randomUUID()}`;
    }

    private createItemId(prefix: string): string {
        return `${prefix}_${randomUUID()}`;
    }

    private normalizeTimestamp(value: unknown): number {
        if (typeof value === 'number' && Number.isFinite(value)) {
            return value;
        }
        return Math.floor(Date.now() / 1000);
    }

    /**
     * Performs a fetch and parses the JSON response, handling errors.
     */
    private async fetchJson<T>(path: string, init: RequestInit): Promise<T> {
        const res = await this.doFetch(path, {
            ...init,
            headers: {
                'Content-Type': 'application/json',
                ...(init.headers || {}),
            },
        });

        const text = await res.text();
        try {
            return JSON.parse(text) as T;
        } catch {
            throw new Error(`Failed to parse response JSON: ${text.substring(0, 200)}`);
        }
    }

    /**
     * Low-level fetch wrapper with error handling.
     */
    private async doFetch(path: string, init: RequestInit): Promise<Response> {
        if (!this.baseUrl) {
            throw new Error('Responses HTTP transport is not available. Start the Foundry Local web service or create the client with a baseUrl.');
        }
        const url = `${this.baseUrl}${path}`;
        let res: Response;
        try {
            res = await fetch(url, init);
        } catch (e) {
            throw new Error(
                `Network error calling ${init.method ?? 'GET'} ${path}: ${e instanceof Error ? e.message : String(e)}`,
                { cause: e }
            );
        }

        if (!res.ok) {
            const errorText = await res.text().catch(() => res.statusText);
            throw new Error(
                `Responses API error (${res.status}): ${errorText}`
            );
        }

        return res;
    }

    /**
     * Parses a Server-Sent Events stream from the fetch response body.
     * Format: "event: {type}\ndata: {json}\n\n"
     * Terminal signal: "data: [DONE]\n\n"
     * Per SSE spec, multiple data: lines within a single event are joined with \n.
     */
    private async parseSSEStream(
        body: ReadableStream<Uint8Array>,
        onEvent: (event: StreamingEvent) => void
    ): Promise<void> {
        const reader = body.getReader();
        const decoder = new TextDecoder();
        const bufferParts: string[] = [];
        let parseError: Error | null = null;

        try {
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                bufferParts.push(decoder.decode(value, { stream: true }));
                const buffer = bufferParts.join('');

                // Process complete SSE blocks (separated by double newlines)
                const blocks = buffer.split('\n\n');
                // Keep the last (potentially incomplete) block for next iteration
                const incomplete = blocks.pop() ?? '';
                bufferParts.length = 0;
                if (incomplete) bufferParts.push(incomplete);

                for (const block of blocks) {
                    if (parseError) break;

                    const trimmed = block.trim();
                    if (!trimmed) continue;

                    // Check for terminal signal
                    if (trimmed === 'data: [DONE]') {
                        return;
                    }

                    // Parse SSE fields — per spec, multiple data: lines are joined with \n
                    const dataLines: string[] = [];
                    for (const line of trimmed.split('\n')) {
                        if (line.startsWith('data: ')) {
                            dataLines.push(line.slice(6));
                        } else if (line === 'data:') {
                            dataLines.push('');
                        }
                        // 'event:' field is informational; the type is inside the JSON data
                    }
                    const eventData = dataLines.length > 0 ? dataLines.join('\n') : undefined;

                    if (eventData) {
                        try {
                            const parsed = JSON.parse(eventData) as StreamingEvent;
                            onEvent(parsed);
                        } catch (e) {
                            parseError = new Error(
                                `Failed to parse streaming event: ${e instanceof Error ? e.message : String(e)}`,
                                { cause: e }
                            );
                        }
                    }
                }
            }
        } finally {
            reader.releaseLock();
        }

        if (parseError) {
            throw parseError;
        }
    }
}
