# Foundry Local REST API Reference

> **⚠️ API UNDER DEVELOPMENT**
>
> This API is actively being developed and may introduce breaking changes without prior notice. We recommend monitoring the changelog for updates before building production applications.

## OpenAI v1 contract

### POST /v1/chat/completions

Handles chat completion requests.  
Compatible with the [OpenAI Chat Completions API](https://platform.openai.com/docs/api-reference/chat/create)

**Request Body:**

_---Properties Defined by OpenAI Contract---_

- `model` (string)  
  The model to use for the completion.
- `messages` (array)  
  A list of messages comprising the conversation history.
  - Each message must contain:
    - `role` (string)  
      The role of the author. Must be one of: `system`, `user`, or `assistant`.
    - `content` (string)  
      The content of the message.
- `temperature` (number, optional)  
  Sampling temperature between 0 and 2. Higher values (e.g., 0.8) produce more random outputs, while lower values (e.g., 0.2) produce more focused, deterministic outputs.
- `top_p` (number, optional)  
  Nucleus sampling probability between 0 and 1. Value of 0.1 means only tokens comprising the top 10% probability mass are considered.
- `n` (integer, optional)  
  Number of chat completion choices to generate for each input message.
- `stream` (boolean, optional)  
  If true, partial message deltas will be sent as server-sent events as they become available, with the stream terminated by a `data: [DONE]` message.
- `stop` (string or array, optional)  
  Up to 4 sequences where the API will stop generating further tokens.
- `max_tokens` (integer, optional)  
  Maximum number of tokens to generate. Deprecated for o1 series models; use `max_completion_tokens` instead.
- `max_completion_token` (integer, optional)  
  Upper bound for the number of tokens to generate, including both visible output tokens and reasoning tokens.
- `presence_penalty` (number, optional)  
  Number between -2.0 and 2.0. Positive values penalize new tokens based on their presence in the text so far, increasing the model's likelihood to talk about new topics.
- `frequency_penalty` (number, optional)  
  Number between -2.0 and 2.0. Positive values penalize new tokens based on their existing frequency in the text, decreasing the model's likelihood to repeat the same line verbatim.
- `logit_bias` (map, optional)  
  Modify the likelihood of specified tokens appearing in the completion.
- `user` (string, optional)  
  A unique identifier representing your end-user, which can help monitor and detect abuse.
- `functions` (array, optional)  
  A list of functions the model may generate JSON inputs for.
  - Each function must include:
    - `name` (string)  
      Function name.
    - `description` (string)  
      Function description.
    - `parameters` (object)  
      Function parameters described as a JSON Schema object.
- `function_call` (string or object, optional)  
  Controls how the model responds to function calls.
  - If object, may include:
    - `name` (string, optional)  
      The name of the function to call.
    - `arguments` (object, optional)  
      The arguments to pass to the function.
- `metadata` (object, optional)  
  A dictionary of metadata key-value pairs.
- `response_format` (object, optional)  
  **Note: Currently, only the default `"text"` format is supported. The `"json_object"` format is not yet supported and will result in an error.**  
  Specifies the format that the model must output. Can be either:
  - `{ "type": "text" }` - (default) model generates natural language responses
  - `{ "type": "json_object" }` - NOT YET SUPPORTED - model guarantees to generate valid JSON

_---Additional Foundry Local Properties---_

- `top_k` (number, optional)  
  The number of highest probability vocabulary tokens to keep for top-k-filtering.
- `random_seed` (integer, optional)  
  Seed for reproducible random number generation.
- `ep` (string, optional)  
  Overwrite the provider for ONNX models. Supports: `"dml"`, `"cuda"`, `"qnn"`, `"cpu"`, `"webgpu"`.
- `ttl` (integer, optional)  
  Time to live in seconds for the model in memory.
- `tools` (object, optional)  
  Tools calculated for the request.

**Response body:**

- `id` (string)  
  Unique identifier for the chat completion.
- `object` (string)  
  The object type, always `"chat.completion"`.
- `created` (integer)  
  Creation timestamp in epoch seconds.
- `model` (string)  
  The model used for completion.
- `choices` (array)  
  List of completion choices, each containing:
  - `index` (integer)  
    The index of this choice.
  - `message` (object)  
    The generated message with:
    - `role` (string)  
      Always `"assistant"` for responses.
    - `content` (string)  
      The actual generated text.
  - `finish_reason` (string)  
    Why generation stopped (e.g., `"stop"`, `"length"`, `"function_call"`).
- `usage` (object)  
  Token usage statistics:
  - `prompt_tokens` (integer)  
    Tokens in the prompt.
  - `completion_tokens` (integer)  
    Tokens in the completion.
  - `total_tokens` (integer)  
    Total tokens used.

**Example:**

- Request body
  ```json
  {
    "model": "phi-4-mini",
    "messages": [
      {
        "role": "user",
        "content": "Hello, how are you?"
      }
    ],
    "temperature": 0.7,
    "top_p": 1,
    "n": 1,
    "stream": false,
    "stop": null,
    "max_tokens": 100,
    "presence_penalty": 0,
    "frequency_penalty": 0,
    "logit_bias": {},
    "user": "user_id_123",
    "functions": [],
    "function_call": null,
    "metadata": {}
  }
  ```
- Response body
  ```json
  {
    "id": "chatcmpl-1234567890",
    "object": "chat.completion",
    "created": 1677851234,
    "model": "phi-4-mini",
    "choices": [
      {
        "index": 0,
        "message": {
          "role": "assistant",
          "content": "I'm doing well, thank you! How can I assist you today?"
        },
        "finish_reason": "stop"
      }
    ],
    "usage": {
      "prompt_tokens": 10,
      "completion_tokens": 20,
      "total_tokens": 30
    }
  }
  ```

### POST /v1/embeddings

Handles embedding generation requests.  
Compatible with the [OpenAI Embeddings API](https://platform.openai.com/docs/api-reference/embeddings/create)

**Request Body:**

- `model` (string)  
  The embedding model to use (e.g., `"text-embedding-ada-002"`).
- `input` (string or array)  
  Input text to embed. Can be a single string or an array of strings/tokens.
- `user` (string, optional)  
  A unique identifier representing your end-user for abuse monitoring.

**Response body:**

- `object` (string)  
  Always `"list"`.
- `data` (array)  
  List of embedding objects, each containing:
  - `object` (string)  
    Always `"embedding"`.
  - `embedding` (array)  
    The vector representation of the input text.
  - `index` (integer)  
    The position of this embedding in the input array.
- `model` (string)  
  The model used for embedding generation.
- `usage` (object)  
  Token usage statistics:
  - `prompt_tokens` (integer)  
    Number of tokens in the prompt.
  - `total_tokens` (integer)  
    Total tokens used.

**Example:**

- Request body
  ```json
  {
    "model": "text-embedding-ada-002",
    "input": "Hello, how are you?",
    "user": "user_id_123"
  }
  ```
- Response body
  ```json
  {
    "object": "list",
    "data": [
      {
        "object": "embedding",
        "embedding": [0.1, 0.2, 0.3, ...],
        "index": 0
      }
    ],
    "model": "text-embedding-ada-002",
    "usage": {
      "prompt_tokens": 10,
      "total_tokens": 10
    }
  }
  ```

## Custom API

### POST /openai/register

Registers an external model provider for use with Foundry Local.

**Request Body:**

- `TypeName` (string)  
  Provider name (e.g., `"deepseek"`)
- `ModelName` (string)  
  Model name to register (e.g., `"deepseek-chat"`)
- `BaseUri` (string)  
  The OpenAI-compatible base URI for the provider

**Response:**

- 200 OK  
  Empty response body

**Example:**

- Request body
  ```json
  {
    "TypeName": "deepseek",
    "ModelName": "deepseek-chat",
    "BaseUri": "https://api.deepseek.com/v1"
  }
  ```

### GET /openai/models

Retrieves all available models, including both local models and registered external models.

**Response:**

- 200 OK  
  An array of model names as strings.

**Example:**

- Response body
  ```json
  ["phi-4-mini", "mistral-7b-v0.2"]
  ```

### GET /openai/load/{name}

Loads a model into memory for faster inference.

**URI Parameters:**

- `name` (string)  
  The model name to load.

**Query Parameters:**

- `unload` (boolean, optional)  
  Whether to automatically unload the model after idle time. Defaults to `true`.
- `ttl` (integer, optional)  
  Time to live in seconds. If greater than 0, overrides `unload` parameter.
- `ep` (string, optional)  
  Execution provider to run this model. Supports: `"dml"`, `"cuda"`, `"qnn"`, `"cpu"`, `"webgpu"`.  
  If not specified, uses settings from `genai_config.json`.

**Response:**

- 200 OK  
  Empty response body

**Example:**

- Request URI
  ```
  GET /openai/load/phi-4-mini?ttl=3600&ep=dml
  ```

### GET /openai/unload/{name}

Unloads a model from memory.

**URI Parameters:**

- `name` (string)  
  The model name to unload.

**Query Parameters:**

- `force` (boolean, optional)  
  If `true`, ignores TTL settings and unloads immediately.

**Response:**

- 200 OK  
  Empty response body

**Example:**

- Request URI
  ```
  GET /openai/unload/phi-4-mini?force=true
  ```

### GET /openai/unloadall

Unloads all models from memory.

**Response:**

- 200 OK  
  Empty response body

### GET /openai/loadedmodels

Retrieves a list of currently loaded models.

**Response:**

- 200 OK  
  An array of model names as strings.

**Example:**

- Response body
  ```json
  ["phi-4-mini", "mistral-7b-v0.2"]
  ```

### GET /openai/getgpudevice

Retrieves the currently selected GPU device ID.

**Response:**

- 200 OK  
  An integer representing the current GPU device ID.

### GET /openai/setgpudevice/{deviceId}

Sets the active GPU device.

**URI Parameters:**

- `deviceId` (integer)  
  The GPU device ID to use.

**Response:**

- 200 OK  
  Empty response body

**Example:**

- Request URI
  ```
  GET /openai/setgpudevice/1
  ```

### POST /openai/download

Downloads a model to local storage.

**Request Body:**

- `model` (string)  
  The model name to download.
- `token` (string, optional)  
  Authentication token for protected models (GitHub or Hugging Face).
- `progressToken` (object, optional)  
  For AITK only. Token to track download progress.
- `customDirPath` (string, optional)  
  Custom download directory (used for CLI, not needed for AITK).
- `bufferSize` (integer, optional)  
  HTTP download buffer size in KB. No effect on NIM or Azure Foundry models.
- `ignorePipeReport` (boolean, optional)  
  If `true`, forces progress reporting via HTTP stream instead of pipe.
  Defaults to `false` for AITK and `true` for Foundry Local.

**Streaming Response:**

During download, the server streams progress updates in the format:

```
("file name", percentage_complete)
```

**Final Response body:**

- `Success` (boolean)  
  Whether the download completed successfully.
- `ErrorMessage` (string, optional)  
  Error details if download failed.

**Example:**

- Request body

  ```json
  {
    "model": "phi-4-mini",
    "ignorePipeReport": true
  }
  ```

- Response stream

  ```
  ("genai_config.json", 0.01)
  ("genai_config.json", 0.2)
  ("model.onnx.data", 0.5)
  ("model.onnx.data", 0.78)
  ...
  ("", 1)
  ```

- Final response
  ```json
  {
    "Success": true,
    "ErrorMessage": null
  }
  ```

### GET /openai/status

Retrieves server status information.

**Response body:**

- `Endpoints` (array of strings)  
  The HTTP server binding endpoints.
- `ModelDirPath` (string)  
  Directory where local models are stored.
- `PipeName` (string)  
  The current NamedPipe server name.

**Example:**

- Response body
  ```json
  {
    "Endpoints": ["http://localhost:5273"],
    "ModelDirPath": "/path/to/models",
    "PipeName": "inference_agent"
  }
  ```

### POST /v1/chat/completions/tokenizer/encode/count

Counts tokens for a given chat completion request without performing inference.

**Request Body:**

- Content-Type: application/json
- JSON object in `ChatCompletionCreateRequest` format with:
  - `model` (string)  
    Model to use for tokenization.
  - `messages` (array)  
    Array of message objects with `role` and `content`.

**Response Body:**

- Content-Type: application/json
- JSON object with token count:
  - `tokenCount` (integer)  
    Number of tokens in the request.

**Example:**

- Request body
  ```json
  {
    "messages": [
      {
        "role": "system",
        "content": "This is a system message"
      },
      {
        "role": "user",
        "content": "Hello, what is Microsoft?"
      }
    ],
    "model": "Phi-4-mini-instruct-cuda-gpu"
  }
  ```
- Response body
  ```json
  {
    "tokenCount": 23
  }
  ```
