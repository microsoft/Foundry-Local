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
    ResponseInputItem,
    MessageItem,
    ContentPart,
    InputImageContent,
} from '../types.js';
import { readFileSync } from 'fs';
import { extname, resolve } from 'path';

const MIME_TYPES: Record<string, string> = {
    '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.png': 'image/png',
    '.gif': 'image/gif', '.webp': 'image/webp', '.bmp': 'image/bmp',
};
const DEFAULT_MAX_IMAGE_DIM = 480;

/**
 * Creates an `input_image` content part from a local file path or URL.
 *
 * Reads the image and encodes it as a base64 data URI. If the source is a URL,
 * it is fetched first. Optionally resizes the image if either dimension exceeds
 * `maxDim` (requires the `sharp` package; skipped if not installed).
 *
 * @param source - A local file path or an `http://`/`https://` URL.
 * @param options - Optional settings.
 * @param options.detail - Image detail level (`"auto"`, `"low"`, or `"high"`). Defaults to `"auto"`.
 * @param options.maxDim - Maximum dimension (width or height) in pixels before resizing. Defaults to 480.
 * @returns A promise resolving to an `InputImageContent` object ready for inclusion in a message's content array.
 *
 * @example
 * ```typescript
 * const content = [
 *     await createImageContent('photo.png'),
 *     { type: 'input_text' as const, text: 'Describe this image' },
 * ];
 * const input = [{ type: 'message' as const, role: 'user' as const, content }];
 * const response = await client.create(input);
 * ```
 */
export async function createImageContent(
    source: string,
    options?: { detail?: string; maxDim?: number },
): Promise<InputImageContent> {
    const detail = options?.detail ?? 'auto';
    const maxDim = options?.maxDim ?? DEFAULT_MAX_IMAGE_DIM;

    let data: Buffer;
    let mime: string;

    if (source.startsWith('http://') || source.startsWith('https://')) {
        const res = await fetch(source);
        if (!res.ok) {
            throw new Error(`Failed to fetch image: HTTP ${res.status}`);
        }
        mime = res.headers.get('content-type')?.split(';')[0].trim() || 'image/jpeg';
        data = Buffer.from(await res.arrayBuffer());
    } else {
        const absPath = resolve(source);
        data = readFileSync(absPath);
        const ext = extname(absPath).toLowerCase();
        mime = MIME_TYPES[ext] || 'image/png';
    }

    // Attempt resize with sharp (optional dependency)
    try {
        const sharp = (await import('sharp')).default;
        const metadata = await sharp(data).metadata();
        const w = metadata.width ?? 0;
        const h = metadata.height ?? 0;
        if (w > maxDim || h > maxDim) {
            const scale = Math.min(maxDim / w, maxDim / h);
            const nw = Math.round(w * scale);
            const nh = Math.round(h * scale);
            data = await sharp(data).resize(nw, nh).png().toBuffer();
            mime = 'image/png';
        }
    } catch {
        // sharp not installed — send at original size
    }

    const b64 = data.toString('base64');
    return {
        type: 'input_image',
        image_url: `data:${mime};base64,${b64}`,
        media_type: mime,
        detail,
    };
}

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
            store: this.store,
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
 * Client for the OpenAI Responses API served by Foundry Local's embedded web service.
 *
 * Unlike ChatClient/AudioClient (which use FFI via CoreInterop), the Responses API
 * is HTTP-only. This client uses fetch() for all operations and parses Server-Sent Events
 * for streaming.
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
    private baseUrl: string;
    private modelId?: string;

    /**
     * Configuration settings for responses.
     */
    public settings = new ResponsesClientSettings();

    /**
     * @param baseUrl - The base URL of the Foundry Local web service (e.g. "http://127.0.0.1:5273").
     * @param modelId - Optional default model ID. Can be overridden per-request via options.
     */
    constructor(baseUrl: string, modelId?: string) {
        if (!baseUrl || typeof baseUrl !== 'string' || baseUrl.trim() === '') {
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

        const response = await this.fetchJson<ResponseObject>(
            '/v1/responses',
            { method: 'POST', body: JSON.stringify(body) }
        );
        return response;
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
        return this.fetchJson<InputItemsListResponse>(
            `/v1/responses/${encodeURIComponent(responseId)}/input_items`,
            { method: 'GET' }
        );
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
