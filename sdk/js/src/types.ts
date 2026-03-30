// adapted from sdk\cs\src\FoundryModelInfo.cs

export enum DeviceType {
    Invalid = 'Invalid',
    CPU = 'CPU',
    GPU = 'GPU',
    NPU = 'NPU'
}

export interface PromptTemplate {
    system?: string | null;
    user?: string | null;
    assistant: string;
    prompt: string;
}

export interface Runtime {
    deviceType: DeviceType;
    executionProvider: string;
}

export interface Parameter {
    name: string;
    value?: string | null;
}

export interface ModelSettings {
    parameters?: Parameter[] | null;
}

export interface ModelInfo {
    id: string;
    name: string;
    version: number;
    alias: string;
    displayName?: string | null;
    providerType: string;
    uri: string;
    modelType: string;
    promptTemplate?: PromptTemplate | null;
    publisher?: string | null;
    modelSettings?: ModelSettings | null;
    license?: string | null;
    licenseDescription?: string | null;
    cached: boolean;
    task?: string | null;
    runtime?: Runtime | null;
    fileSizeMb?: number | null;
    supportsToolCalling?: boolean | null;
    maxOutputTokens?: number | null;
    minFLVersion?: string | null;
    createdAtUnix: number;
    contextLength?: number | null;
    inputModalities?: string | null;
    outputModalities?: string | null;
    capabilities?: string | null;
}

export interface ResponseFormat {
    type: string;
    jsonSchema?: string;
    larkGrammar?: string;
}

export interface ToolChoice {
    type: string;
    name?: string;
}

// ============================================================================
// Responses API Types
// Aligned with OpenAI Responses API / OpenResponses spec and
// neutron-server src/FoundryLocalCore/Core/Responses/Contracts/
// ============================================================================

/** Status of a Response object. */
export type ResponseStatus = 'queued' | 'in_progress' | 'completed' | 'failed' | 'incomplete' | 'cancelled';

/** Role of a message in the Responses API. */
export type MessageRole = 'system' | 'user' | 'assistant' | 'developer';

/** Status of an individual response item. */
export type ResponseItemStatus = 'in_progress' | 'completed' | 'incomplete';

/** Controls which tool the model should use. */
export type ResponseToolChoice = 'none' | 'auto' | 'required' | ResponseToolChoiceFunction;

export interface ResponseToolChoiceFunction {
    type: 'function';
    name: string;
}

/** Truncation strategy. */
export type TruncationStrategy = 'auto' | 'disabled';

/** Service tier. */
export type ServiceTier = 'default' | 'auto' | 'flex' | 'priority';

// --- Content Parts ---

export interface InputTextContent {
    type: 'input_text';
    text: string;
}

export interface OutputTextContent {
    type: 'output_text';
    text: string;
    annotations?: Annotation[];
    logprobs?: LogProb[];
}

export interface RefusalContent {
    type: 'refusal';
    refusal: string;
}

export type ContentPart = InputTextContent | OutputTextContent | RefusalContent;

export interface Annotation {
    type: string;
    start_index: number;
    end_index: number;
}

export interface UrlCitationAnnotation extends Annotation {
    type: 'url_citation';
    url: string;
    title: string;
}

export interface LogProb {
    token: string;
    logprob: number;
    bytes?: number[];
}

// --- Function Tools ---

export interface FunctionToolDefinition {
    type: 'function';
    name: string;
    description?: string;
    parameters?: Record<string, unknown>;
    strict?: boolean;
}

// --- Response Items (input & output) ---

export interface MessageItem {
    type: 'message';
    id?: string;
    role: MessageRole;
    content: string | ContentPart[];
    status?: ResponseItemStatus;
}

export interface FunctionCallItem {
    type: 'function_call';
    id?: string;
    call_id: string;
    name: string;
    arguments: string;
    status?: ResponseItemStatus;
}

export interface FunctionCallOutputItem {
    type: 'function_call_output';
    id?: string;
    call_id: string;
    output: string | ContentPart[];
    status?: ResponseItemStatus;
}

export interface ItemReference {
    type: 'item_reference';
    id: string;
}

export interface ReasoningItem {
    type: 'reasoning';
    id?: string;
    content?: ContentPart[];
    encrypted_content?: string;
    summary?: string;
    status?: ResponseItemStatus;
}

export type ResponseInputItem = MessageItem | FunctionCallItem | FunctionCallOutputItem | ItemReference | ReasoningItem;
export type ResponseOutputItem = MessageItem | FunctionCallItem | ReasoningItem;

// --- Reasoning & Text Config ---

export interface ReasoningConfig {
    effort?: string;
    summary?: string;
}

export interface TextFormat {
    type: string;
    name?: string;
    description?: string;
    schema?: unknown;
    strict?: boolean;
}

export interface TextConfig {
    format?: TextFormat;
    verbosity?: string;
}

// --- Response Usage ---

export interface ResponseUsage {
    input_tokens: number;
    output_tokens: number;
    total_tokens: number;
    input_tokens_details?: { cached_tokens: number };
    output_tokens_details?: { reasoning_tokens: number };
}

// --- Response Error ---

export interface ResponseError {
    code: string;
    message: string;
}

export interface IncompleteDetails {
    reason: string;
}

// --- Response Create Request ---

export interface ResponseCreateParams {
    model?: string;
    input?: string | ResponseInputItem[];
    instructions?: string;
    previous_response_id?: string;
    tools?: FunctionToolDefinition[];
    tool_choice?: ResponseToolChoice;
    temperature?: number;
    top_p?: number;
    max_output_tokens?: number;
    frequency_penalty?: number;
    presence_penalty?: number;
    truncation?: TruncationStrategy;
    parallel_tool_calls?: boolean;
    store?: boolean;
    metadata?: Record<string, string>;
    stream?: boolean;
    reasoning?: ReasoningConfig;
    text?: TextConfig;
    seed?: number;
    user?: string;
}

// --- Response Object ---

export interface ResponseObject {
    id: string;
    object: 'response';
    created_at: number;
    completed_at?: number | null;
    failed_at?: number | null;
    cancelled_at?: number | null;
    status: ResponseStatus;
    incomplete_details?: IncompleteDetails | null;
    model: string;
    previous_response_id?: string | null;
    instructions?: string | null;
    output: ResponseOutputItem[];
    error?: ResponseError | null;
    tools: FunctionToolDefinition[];
    tool_choice: ResponseToolChoice;
    truncation: TruncationStrategy;
    parallel_tool_calls: boolean;
    text: TextConfig;
    top_p: number;
    temperature: number;
    presence_penalty: number;
    frequency_penalty: number;
    max_output_tokens?: number | null;
    reasoning?: ReasoningConfig | null;
    store: boolean;
    metadata?: Record<string, string> | null;
    usage?: ResponseUsage | null;
    user?: string | null;
}

// --- Input Items List Response ---

export interface InputItemsListResponse {
    object: 'list';
    data: ResponseInputItem[];
}

// --- Delete Response ---

export interface DeleteResponseResult {
    id: string;
    object: string;
    deleted: boolean;
}

// --- Streaming Events ---
// Scoped to events emitted by neutron-server (StreamingEvents.cs)

export interface ResponseLifecycleEvent {
    type: 'response.created' | 'response.queued' | 'response.in_progress' | 'response.completed' | 'response.failed' | 'response.incomplete';
    response: ResponseObject;
    sequence_number: number;
}

export interface OutputItemAddedEvent {
    type: 'response.output_item.added';
    item_id: string;
    output_index: number;
    item: ResponseOutputItem;
    sequence_number: number;
}

export interface OutputItemDoneEvent {
    type: 'response.output_item.done';
    item_id: string;
    output_index: number;
    item: ResponseOutputItem;
    sequence_number: number;
}

export interface ContentPartAddedEvent {
    type: 'response.content_part.added';
    item_id: string;
    content_index: number;
    part: ContentPart;
    sequence_number: number;
}

export interface ContentPartDoneEvent {
    type: 'response.content_part.done';
    item_id: string;
    content_index: number;
    part: ContentPart;
    sequence_number: number;
}

export interface OutputTextDeltaEvent {
    type: 'response.output_text.delta';
    item_id: string;
    output_index: number;
    content_index: number;
    delta: string;
    sequence_number: number;
}

export interface OutputTextDoneEvent {
    type: 'response.output_text.done';
    item_id: string;
    output_index: number;
    content_index: number;
    text: string;
    sequence_number: number;
}

export interface RefusalDeltaEvent {
    type: 'response.refusal.delta';
    item_id: string;
    content_index: number;
    delta: string;
    sequence_number: number;
}

export interface RefusalDoneEvent {
    type: 'response.refusal.done';
    item_id: string;
    content_index: number;
    refusal: string;
    sequence_number: number;
}

export interface FunctionCallArgsDeltaEvent {
    type: 'response.function_call_arguments.delta';
    item_id: string;
    output_index: number;
    delta: string;
    sequence_number: number;
}

export interface FunctionCallArgsDoneEvent {
    type: 'response.function_call_arguments.done';
    item_id: string;
    output_index: number;
    arguments: string;
    name: string;
    sequence_number: number;
}

export interface StreamingErrorEvent {
    type: 'error';
    code?: string;
    message?: string;
    param?: string;
    sequence_number: number;
}

export type StreamingEvent =
    | ResponseLifecycleEvent
    | OutputItemAddedEvent
    | OutputItemDoneEvent
    | ContentPartAddedEvent
    | ContentPartDoneEvent
    | OutputTextDeltaEvent
    | OutputTextDoneEvent
    | RefusalDeltaEvent
    | RefusalDoneEvent
    | FunctionCallArgsDeltaEvent
    | FunctionCallArgsDoneEvent
    | StreamingErrorEvent;
