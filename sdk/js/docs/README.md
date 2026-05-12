# foundry-local-sdk

## Enumerations

### DeviceType

#### Enumeration Members

| Enumeration Member | Value |
| ------ | ------ |
| <a id="enumeration-member-cpu"></a> `CPU` | `"CPU"` |
| <a id="enumeration-member-gpu"></a> `GPU` | `"GPU"` |
| <a id="enumeration-member-invalid"></a> `Invalid` | `"Invalid"` |
| <a id="enumeration-member-npu"></a> `NPU` | `"NPU"` |

## Classes

- [AudioClient](classes/AudioClient.md)
- [AudioClientSettings](classes/AudioClientSettings.md)
- [Catalog](classes/Catalog.md)
- [ChatClient](classes/ChatClient.md)
- [ChatClientSettings](classes/ChatClientSettings.md)
- [EmbeddingClient](classes/EmbeddingClient.md)
- [FoundryLocalManager](classes/FoundryLocalManager.md)
- [Model](classes/Model.md)
- [ModelLoadManager](classes/ModelLoadManager.md)
- [ResponsesClient](classes/ResponsesClient.md)
- [ResponsesClientSettings](classes/ResponsesClientSettings.md)

## Interfaces

### Annotation

#### Extended by

- [`UrlCitationAnnotation`](#urlcitationannotation)

#### Properties

##### end\_index

```ts
end_index: number;
```

##### start\_index

```ts
start_index: number;
```

##### type

```ts
type: string;
```

***

### ContentPartAddedEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### item\_id

```ts
item_id: string;
```

##### part

```ts
part: ContentPart;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.content_part.added";
```

***

### ContentPartDoneEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### item\_id

```ts
item_id: string;
```

##### part

```ts
part: ContentPart;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.content_part.done";
```

***

### DeleteResponseResult

#### Properties

##### deleted

```ts
deleted: boolean;
```

##### id

```ts
id: string;
```

##### object

```ts
object: string;
```

***

### EpDownloadResult

Result of an explicit EP download and registration operation.

#### Properties

##### failedEps

```ts
failedEps: string[];
```

Names of EPs that failed to register.

##### registeredEps

```ts
registeredEps: string[];
```

Names of EPs that were successfully registered.

##### status

```ts
status: string;
```

Human-readable status message.

##### success

```ts
success: boolean;
```

True if all requested EPs were successfully downloaded and registered.

***

### EpInfo

Describes a discoverable execution provider bootstrapper.

#### Properties

##### isRegistered

```ts
isRegistered: boolean;
```

True if this EP has already been successfully downloaded and registered.

##### name

```ts
name: string;
```

The identifier of the bootstrapper/execution provider (e.g. "CUDAExecutionProvider").

***

### FoundryLocalConfig

Configuration options for the Foundry Local SDK.
Use a plain object with these properties to configure the SDK.

#### Properties

##### additionalSettings?

```ts
optional additionalSettings?: {
[key: string]: string;
};
```

Additional settings to pass to the core.
Optional. Internal use only.

###### Index Signature

```ts
[key: string]: string
```

##### appDataDir?

```ts
optional appDataDir?: string;
```

The directory where application data should be stored.
Optional. Defaults to `{user_home}/.{appName}`.

##### appName

```ts
appName: string;
```

**REQUIRED** The name of the application using the SDK.
Used for identifying the application in logs and telemetry.

##### libraryPath?

```ts
optional libraryPath?: string;
```

The path to the directory containing the native Foundry Local Core libraries.
Optional. This directory must contain `Microsoft.AI.Foundry.Local.Core`, `onnxruntime`, and `onnxruntime-genai` binaries.
If not provided, the SDK attempts to discover them in standard locations.

##### logLevel?

```ts
optional logLevel?: "trace" | "debug" | "info" | "warn" | "error" | "fatal";
```

The logging level for the SDK.
Optional. Valid values: 'trace', 'debug', 'info', 'warn', 'error', 'fatal'.
Defaults to 'warn'.

##### logsDir?

```ts
optional logsDir?: string;
```

The directory where log files are written.
Optional. Defaults to `{appDataDir}/logs`.

##### modelCacheDir?

```ts
optional modelCacheDir?: string;
```

The directory where models are downloaded and cached.
Optional. Defaults to `{appDataDir}/cache/models`.

##### serviceEndpoint?

```ts
optional serviceEndpoint?: string;
```

The external URL if the web service is running in a separate process.
Optional. This is used to connect to an existing service instance.

##### webServiceUrls?

```ts
optional webServiceUrls?: string;
```

The URL(s) for the local web service to bind to.
Optional. Multiple URLs can be separated by semicolons.
Example: "http://127.0.0.1:8080"

***

### FunctionCallArgsDeltaEvent

#### Properties

##### delta

```ts
delta: string;
```

##### item\_id

```ts
item_id: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.function_call_arguments.delta";
```

***

### FunctionCallArgsDoneEvent

#### Properties

##### arguments

```ts
arguments: string;
```

##### item\_id

```ts
item_id: string;
```

##### name

```ts
name: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.function_call_arguments.done";
```

***

### FunctionCallItem

#### Properties

##### arguments

```ts
arguments: string;
```

##### call\_id

```ts
call_id: string;
```

##### id?

```ts
optional id?: string;
```

##### name

```ts
name: string;
```

##### status?

```ts
optional status?: ResponseItemStatus;
```

##### type

```ts
type: "function_call";
```

***

### FunctionCallOutputItem

#### Properties

##### call\_id

```ts
call_id: string;
```

##### id?

```ts
optional id?: string;
```

##### output

```ts
output: string | ContentPart[];
```

##### status?

```ts
optional status?: ResponseItemStatus;
```

##### type

```ts
type: "function_call_output";
```

***

### FunctionToolDefinition

#### Properties

##### description?

```ts
optional description?: string;
```

##### name

```ts
name: string;
```

##### parameters?

```ts
optional parameters?: Record<string, unknown>;
```

##### strict?

```ts
optional strict?: boolean;
```

##### type

```ts
type: "function";
```

***

### IModel

#### Accessors

##### alias

###### Get Signature

```ts
get alias(): string;
```

###### Returns

`string`

##### capabilities

###### Get Signature

```ts
get capabilities(): string | null;
```

###### Returns

`string` \| `null`

##### contextLength

###### Get Signature

```ts
get contextLength(): number | null;
```

###### Returns

`number` \| `null`

##### id

###### Get Signature

```ts
get id(): string;
```

###### Returns

`string`

##### info

###### Get Signature

```ts
get info(): ModelInfo;
```

###### Returns

[`ModelInfo`](#modelinfo)

##### inputModalities

###### Get Signature

```ts
get inputModalities(): string | null;
```

###### Returns

`string` \| `null`

##### isCached

###### Get Signature

```ts
get isCached(): boolean;
```

###### Returns

`boolean`

##### outputModalities

###### Get Signature

```ts
get outputModalities(): string | null;
```

###### Returns

`string` \| `null`

##### path

###### Get Signature

```ts
get path(): string;
```

###### Returns

`string`

##### supportsToolCalling

###### Get Signature

```ts
get supportsToolCalling(): boolean | null;
```

###### Returns

`boolean` \| `null`

##### variants

###### Get Signature

```ts
get variants(): IModel[];
```

Variants of the model that are available. Variants of the model are optimized for different devices.

###### Returns

[`IModel`](#imodel)[]

#### Methods

##### createAudioClient()

```ts
createAudioClient(): AudioClient;
```

###### Returns

[`AudioClient`](classes/AudioClient.md)

##### createChatClient()

```ts
createChatClient(): ChatClient;
```

###### Returns

[`ChatClient`](classes/ChatClient.md)

##### createResponsesClient()

```ts
createResponsesClient(baseUrl): ResponsesClient;
```

Creates a ResponsesClient for interacting with the model via the Responses API.
Unlike createChatClient/createAudioClient (which use FFI), the Responses API
is HTTP-based, so the web service base URL must be provided.

###### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `baseUrl` | `string` | The base URL of the Foundry Local web service. |

###### Returns

[`ResponsesClient`](classes/ResponsesClient.md)

##### download()

```ts
download(progressCallback?): Promise<void>;
```

###### Parameters

| Parameter | Type |
| ------ | ------ |
| `progressCallback?` | (`progress`) => `void` |

###### Returns

`Promise`\<`void`\>

##### isLoaded()

```ts
isLoaded(): Promise<boolean>;
```

###### Returns

`Promise`\<`boolean`\>

##### load()

```ts
load(): Promise<void>;
```

###### Returns

`Promise`\<`void`\>

##### removeFromCache()

```ts
removeFromCache(): void;
```

###### Returns

`void`

##### selectVariant()

```ts
selectVariant(variant): void;
```

Select a model variant from variants to use for IModel operations.
An IModel from `variants` can also be used directly.

###### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `variant` | [`IModel`](#imodel) | Model variant to select. Must be one of the variants in `variants`. |

###### Returns

`void`

###### Throws

Error if variant is not valid for this model.

##### unload()

```ts
unload(): Promise<void>;
```

###### Returns

`Promise`\<`void`\>

***

### IncompleteDetails

#### Properties

##### reason

```ts
reason: string;
```

***

### InputItemsListResponse

#### Properties

##### data

```ts
data: ResponseInputItem[];
```

##### object

```ts
object: "list";
```

***

### InputTextContent

#### Properties

##### text

```ts
text: string;
```

##### type

```ts
type: "input_text";
```

***

### ItemReference

#### Properties

##### id

```ts
id: string;
```

##### type

```ts
type: "item_reference";
```

***

### LogProb

#### Properties

##### bytes?

```ts
optional bytes?: number[];
```

##### logprob

```ts
logprob: number;
```

##### token

```ts
token: string;
```

***

### MessageItem

#### Properties

##### content

```ts
content: string | ContentPart[];
```

##### id?

```ts
optional id?: string;
```

##### role

```ts
role: MessageRole;
```

##### status?

```ts
optional status?: ResponseItemStatus;
```

##### type

```ts
type: "message";
```

***

### ModelInfo

#### Properties

##### alias

```ts
alias: string;
```

##### cached

```ts
cached: boolean;
```

##### capabilities?

```ts
optional capabilities?: string | null;
```

##### contextLength?

```ts
optional contextLength?: number | null;
```

##### createdAtUnix

```ts
createdAtUnix: number;
```

##### displayName?

```ts
optional displayName?: string | null;
```

##### fileSizeMb?

```ts
optional fileSizeMb?: number | null;
```

##### id

```ts
id: string;
```

##### inputModalities?

```ts
optional inputModalities?: string | null;
```

##### license?

```ts
optional license?: string | null;
```

##### licenseDescription?

```ts
optional licenseDescription?: string | null;
```

##### maxOutputTokens?

```ts
optional maxOutputTokens?: number | null;
```

##### minFLVersion?

```ts
optional minFLVersion?: string | null;
```

##### modelSettings?

```ts
optional modelSettings?: ModelSettings | null;
```

##### modelType

```ts
modelType: string;
```

##### name

```ts
name: string;
```

##### outputModalities?

```ts
optional outputModalities?: string | null;
```

##### promptTemplate?

```ts
optional promptTemplate?: PromptTemplate | null;
```

##### providerType

```ts
providerType: string;
```

##### publisher?

```ts
optional publisher?: string | null;
```

##### runtime?

```ts
optional runtime?: Runtime | null;
```

##### supportsToolCalling?

```ts
optional supportsToolCalling?: boolean | null;
```

##### task?

```ts
optional task?: string | null;
```

##### uri

```ts
uri: string;
```

##### version

```ts
version: number;
```

***

### ModelSettings

#### Properties

##### parameters?

```ts
optional parameters?: Parameter[] | null;
```

***

### OutputItemAddedEvent

#### Properties

##### item

```ts
item: ResponseOutputItem;
```

##### item\_id

```ts
item_id: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.output_item.added";
```

***

### OutputItemDoneEvent

#### Properties

##### item

```ts
item: ResponseOutputItem;
```

##### item\_id

```ts
item_id: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.output_item.done";
```

***

### OutputTextContent

#### Properties

##### annotations?

```ts
optional annotations?: Annotation[];
```

##### logprobs?

```ts
optional logprobs?: LogProb[];
```

##### text

```ts
text: string;
```

##### type

```ts
type: "output_text";
```

***

### OutputTextDeltaEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### delta

```ts
delta: string;
```

##### item\_id

```ts
item_id: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.output_text.delta";
```

***

### OutputTextDoneEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### item\_id

```ts
item_id: string;
```

##### output\_index

```ts
output_index: number;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### text

```ts
text: string;
```

##### type

```ts
type: "response.output_text.done";
```

***

### Parameter

#### Properties

##### name

```ts
name: string;
```

##### value?

```ts
optional value?: string | null;
```

***

### PromptTemplate

#### Properties

##### assistant

```ts
assistant: string;
```

##### prompt

```ts
prompt: string;
```

##### system?

```ts
optional system?: string | null;
```

##### user?

```ts
optional user?: string | null;
```

***

### ReasoningConfig

#### Properties

##### effort?

```ts
optional effort?: string;
```

##### summary?

```ts
optional summary?: string;
```

***

### ReasoningItem

#### Properties

##### content?

```ts
optional content?: ContentPart[];
```

##### encrypted\_content?

```ts
optional encrypted_content?: string;
```

##### id?

```ts
optional id?: string;
```

##### status?

```ts
optional status?: ResponseItemStatus;
```

##### summary?

```ts
optional summary?: string;
```

##### type

```ts
type: "reasoning";
```

***

### RefusalContent

#### Properties

##### refusal

```ts
refusal: string;
```

##### type

```ts
type: "refusal";
```

***

### RefusalDeltaEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### delta

```ts
delta: string;
```

##### item\_id

```ts
item_id: string;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.refusal.delta";
```

***

### RefusalDoneEvent

#### Properties

##### content\_index

```ts
content_index: number;
```

##### item\_id

```ts
item_id: string;
```

##### refusal

```ts
refusal: string;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "response.refusal.done";
```

***

### ResponseCreateParams

#### Properties

##### frequency\_penalty?

```ts
optional frequency_penalty?: number;
```

##### input?

```ts
optional input?: string | ResponseInputItem[];
```

##### instructions?

```ts
optional instructions?: string;
```

##### max\_output\_tokens?

```ts
optional max_output_tokens?: number;
```

##### metadata?

```ts
optional metadata?: Record<string, string>;
```

##### model?

```ts
optional model?: string;
```

##### parallel\_tool\_calls?

```ts
optional parallel_tool_calls?: boolean;
```

##### presence\_penalty?

```ts
optional presence_penalty?: number;
```

##### previous\_response\_id?

```ts
optional previous_response_id?: string;
```

##### reasoning?

```ts
optional reasoning?: ReasoningConfig;
```

##### seed?

```ts
optional seed?: number;
```

##### store?

```ts
optional store?: boolean;
```

##### stream?

```ts
optional stream?: boolean;
```

##### temperature?

```ts
optional temperature?: number;
```

##### text?

```ts
optional text?: TextConfig;
```

##### tool\_choice?

```ts
optional tool_choice?: ResponseToolChoice;
```

##### tools?

```ts
optional tools?: FunctionToolDefinition[];
```

##### top\_p?

```ts
optional top_p?: number;
```

##### truncation?

```ts
optional truncation?: TruncationStrategy;
```

##### user?

```ts
optional user?: string;
```

***

### ResponseError

#### Properties

##### code

```ts
code: string;
```

##### message

```ts
message: string;
```

***

### ResponseFormat

#### Properties

##### jsonSchema?

```ts
optional jsonSchema?: string;
```

##### larkGrammar?

```ts
optional larkGrammar?: string;
```

##### type

```ts
type: string;
```

***

### ResponseLifecycleEvent

#### Properties

##### response

```ts
response: ResponseObject;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: 
  | "response.created"
  | "response.queued"
  | "response.in_progress"
  | "response.completed"
  | "response.failed"
  | "response.incomplete";
```

***

### ResponseObject

#### Properties

##### cancelled\_at?

```ts
optional cancelled_at?: number | null;
```

##### completed\_at?

```ts
optional completed_at?: number | null;
```

##### created\_at

```ts
created_at: number;
```

##### error?

```ts
optional error?: ResponseError | null;
```

##### failed\_at?

```ts
optional failed_at?: number | null;
```

##### frequency\_penalty

```ts
frequency_penalty: number;
```

##### id

```ts
id: string;
```

##### incomplete\_details?

```ts
optional incomplete_details?: IncompleteDetails | null;
```

##### instructions?

```ts
optional instructions?: string | null;
```

##### max\_output\_tokens?

```ts
optional max_output_tokens?: number | null;
```

##### metadata?

```ts
optional metadata?: Record<string, string> | null;
```

##### model

```ts
model: string;
```

##### object

```ts
object: "response";
```

##### output

```ts
output: ResponseOutputItem[];
```

##### parallel\_tool\_calls

```ts
parallel_tool_calls: boolean;
```

##### presence\_penalty

```ts
presence_penalty: number;
```

##### previous\_response\_id?

```ts
optional previous_response_id?: string | null;
```

##### reasoning?

```ts
optional reasoning?: ReasoningConfig | null;
```

##### status

```ts
status: ResponseStatus;
```

##### store

```ts
store: boolean;
```

##### temperature

```ts
temperature: number;
```

##### text

```ts
text: TextConfig;
```

##### tool\_choice

```ts
tool_choice: ResponseToolChoice;
```

##### tools

```ts
tools: FunctionToolDefinition[];
```

##### top\_p

```ts
top_p: number;
```

##### truncation

```ts
truncation: TruncationStrategy;
```

##### usage?

```ts
optional usage?: ResponseUsage | null;
```

##### user?

```ts
optional user?: string | null;
```

***

### ResponseToolChoiceFunction

#### Properties

##### name

```ts
name: string;
```

##### type

```ts
type: "function";
```

***

### ResponseUsage

#### Properties

##### input\_tokens

```ts
input_tokens: number;
```

##### input\_tokens\_details?

```ts
optional input_tokens_details?: {
  cached_tokens: number;
};
```

###### cached\_tokens

```ts
cached_tokens: number;
```

##### output\_tokens

```ts
output_tokens: number;
```

##### output\_tokens\_details?

```ts
optional output_tokens_details?: {
  reasoning_tokens: number;
};
```

###### reasoning\_tokens

```ts
reasoning_tokens: number;
```

##### total\_tokens

```ts
total_tokens: number;
```

***

### Runtime

#### Properties

##### deviceType

```ts
deviceType: DeviceType;
```

##### executionProvider

```ts
executionProvider: string;
```

***

### StreamingErrorEvent

#### Properties

##### code?

```ts
optional code?: string;
```

##### message?

```ts
optional message?: string;
```

##### param?

```ts
optional param?: string;
```

##### sequence\_number

```ts
sequence_number: number;
```

##### type

```ts
type: "error";
```

***

### TextConfig

#### Properties

##### format?

```ts
optional format?: TextFormat;
```

##### verbosity?

```ts
optional verbosity?: string;
```

***

### TextFormat

#### Properties

##### description?

```ts
optional description?: string;
```

##### name?

```ts
optional name?: string;
```

##### schema?

```ts
optional schema?: unknown;
```

##### strict?

```ts
optional strict?: boolean;
```

##### type

```ts
type: string;
```

***

### ToolChoice

#### Properties

##### name?

```ts
optional name?: string;
```

##### type

```ts
type: string;
```

***

### UrlCitationAnnotation

#### Extends

- [`Annotation`](#annotation)

#### Properties

##### end\_index

```ts
end_index: number;
```

###### Inherited from

[`Annotation`](#annotation).[`end_index`](#end_index)

##### start\_index

```ts
start_index: number;
```

###### Inherited from

[`Annotation`](#annotation).[`start_index`](#start_index)

##### title

```ts
title: string;
```

##### type

```ts
type: "url_citation";
```

###### Overrides

[`Annotation`](#annotation).[`type`](#type)

##### url

```ts
url: string;
```

## Type Aliases

### ContentPart

```ts
type ContentPart = 
  | InputTextContent
  | OutputTextContent
  | RefusalContent;
```

***

### MessageRole

```ts
type MessageRole = "system" | "user" | "assistant" | "developer";
```

Role of a message in the Responses API.

***

### ResponseInputItem

```ts
type ResponseInputItem = 
  | MessageItem
  | FunctionCallItem
  | FunctionCallOutputItem
  | ItemReference
  | ReasoningItem;
```

***

### ResponseItemStatus

```ts
type ResponseItemStatus = "in_progress" | "completed" | "incomplete";
```

Status of an individual response item.

***

### ResponseOutputItem

```ts
type ResponseOutputItem = 
  | MessageItem
  | FunctionCallItem
  | ReasoningItem;
```

***

### ResponseStatus

```ts
type ResponseStatus = 
  | "queued"
  | "in_progress"
  | "completed"
  | "failed"
  | "incomplete"
  | "cancelled";
```

Status of a Response object.

***

### ResponseToolChoice

```ts
type ResponseToolChoice = 
  | "none"
  | "auto"
  | "required"
  | ResponseToolChoiceFunction;
```

Controls which tool the model should use.

***

### ServiceTier

```ts
type ServiceTier = "default" | "auto" | "flex" | "priority";
```

Service tier.

***

### StreamingEvent

```ts
type StreamingEvent = 
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
```

***

### TruncationStrategy

```ts
type TruncationStrategy = "auto" | "disabled";
```

Truncation strategy.

## Functions

### getOutputText()

```ts
function getOutputText(response): string;
```

Extracts the text content from an assistant message in a Response.
Equivalent to OpenAI Python SDK's `response.output_text`.

#### Parameters

| Parameter | Type | Description |
| ------ | ------ | ------ |
| `response` | [`ResponseObject`](#responseobject) | The Response object. |

#### Returns

`string`

The concatenated text from the first assistant message, or an empty string.
