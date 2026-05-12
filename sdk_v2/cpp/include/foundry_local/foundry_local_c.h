// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <stddef.h>
#include <stdint.h>

// SAL2 Definitions
#ifndef _MSC_VER
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _Out_
#define _Out_opt_
#define _Outptr_
#define _Outptr_opt_
#define _Inout_
#define _Inout_opt_
#define _Frees_ptr_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Check_return_
#define _Outptr_result_maybenull_
#define _Outptr_result_maybenull_z_
#define _In_reads_(X)
#define _In_reads_opt_
#define _Inout_updates_(X)
#define _Out_writes_(X)
#define _Out_writes_opt_(X)
#define _Inout_updates_all_(X)
#define _Out_writes_bytes_all_(X)
#define _Out_writes_all_(X)
#define _Success_(X)
#define _Outptr_result_buffer_maybenull_(X)
#define ORT_ALL_ARGS_NONNULL __attribute__((nonnull))
#else
#include <specstrings.h>
#define ORT_ALL_ARGS_NONNULL
#endif

/* -----------------------------------------------------------------------
 * Foundry Local SDK API Version
 * Incremented with each release.
 * Used to request the API function table via FoundryLocalGetApi.
 * ----------------------------------------------------------------------- */
#define FOUNDRY_LOCAL_API_VERSION 1

/* -----------------------------------------------------------------------
 * Platform export macros (C version)
 * ----------------------------------------------------------------------- */
#if defined(_WIN32)
#if defined(FL_BUILDING_SHARED_LIBRARY)
#define FL_EXPORT __declspec(dllexport)
#elif defined(FL_STATIC_LIBRARY)
#define FL_EXPORT
#else
#define FL_EXPORT __declspec(dllimport)
#endif

#define FL_API_CALL __stdcall
#define FL_MUST_USE_RESULT
#else
// GCC/Clang: mark exported symbols with default visibility so they remain
// exported even when the library is built with -fvisibility=hidden.
#define FL_EXPORT __attribute__((visibility("default")))

#define FL_API_CALL
#define FL_MUST_USE_RESULT __attribute__((warn_unused_result))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define FL_NO_EXCEPTION noexcept
#else
#define FL_NO_EXCEPTION
#endif

#define FL_TYPE(X) \
  struct fl##X;    \
  typedef struct fl##X fl##X

// Opaque type declarations
FL_TYPE(Api);
FL_TYPE(Catalog);

// Session input/output types
// Need to support generative and predictive, including scenarios where model package provides pre/post processing to a
// predictive model.
// Tensor, text, audio, image. Possibly bytes for realtime audio.
// C++ API can wrap this and provide InputItem/OutputItem/etc. if needed
FL_TYPE(Configuration);
FL_TYPE(Item);
// Queue of items for use in a callback. The producer and consumer threads may differ and change, so we need a queue
// to synchronize. The queue is thread safe and supports multiple producers and consumers.
FL_TYPE(ItemQueue);

FL_TYPE(KeyValuePairs);  // generic key/value pairs. value is optional
FL_TYPE(Manager);
FL_TYPE(Model);
FL_TYPE(ModelInfo);
FL_TYPE(ModelList);

// Generic opaque request/response at this level. Can add more specific things similar to Item if needed.
// C++ API can provide scenario specific types for the user by wrapping these.
// e.g. C++ could have a ChatRequest type that wraps the opaque Request and adds requirements to input/output items and parameters.
// Request accumulates parameters and input items
// Response accumulates output items
FL_TYPE(Request);
FL_TYPE(Response);

// Opaque type for a session. Create with loaded Model so Model:Session is 1:M
// The FL Session contains state as needed.
// A FL session with a GenAI model can cache the generator for continuous decoding, and maintain previous input/output
// messages for Responses API usage.
FL_TYPE(Session);

FL_TYPE(Status);

#define FL_TYPE_RELEASE(X) void(FL_API_CALL * X##_Release)(_Frees_ptr_opt_ fl##X * instance) FL_NO_EXCEPTION

#ifdef _MSC_VER
typedef _Return_type_success_(return == 0) flStatus* flStatusPtr;
#else
typedef flStatus* flStatusPtr;
#endif

// Usage: return_type FL_API_T(Name, param0, ...)
// Defines a function pointer returning an arbitrary type.
// API functions should never throw. Exception must be caught in the implementation and returned as a status.
#define FL_API_T(NAME, ...) (FL_API_CALL * NAME)(__VA_ARGS__) FL_NO_EXCEPTION

// Defines a function pointer returning a FoundryLocalStatus*. Result must be checked. Non-null is an error.
#define FL_API_STATUS(NAME, ...) \
  _Check_return_ _Ret_maybenull_ flStatusPtr(FL_API_CALL* NAME)(__VA_ARGS__) FL_NO_EXCEPTION FL_MUST_USE_RESULT

// Used in *.cc files to match the FL_API_STATUS from the declaration.
#define FL_API_STATUS_IMPL(NAME, ...) \
  _Success_(return == 0) _Check_return_ _Ret_maybenull_ flStatusPtr FL_API_CALL NAME(__VA_ARGS__) FL_NO_EXCEPTION

#ifdef __DOXYGEN__
#undef FL_API_STATUS
#define FL_API_STATUS(NAME, ...) flStatus* NAME(__VA_ARGS__)
#undef FL_TYPE_RELEASE
#define FL_TYPE_RELEASE(X) void Release##X(FL##X* input)
#undef NO_EXCEPTION
#define NO_EXCEPTION
#endif

/* -----------------------------------------------------------------------
 * Exported symbols — these are the ONLY symbols the library exports
 * ----------------------------------------------------------------------- */

/** Get the API function table for the requested version. Returns NULL if unsupported. */
FL_EXPORT const flApi* FL_API_CALL FoundryLocalGetApi(uint32_t version) FL_NO_EXCEPTION;

/** Returns the library version string. */
FL_EXPORT const char* FL_API_CALL FoundryLocalGetVersionString(void) FL_NO_EXCEPTION;

typedef enum flErrorCode {
  FOUNDRY_LOCAL_OK = 0,
  FOUNDRY_LOCAL_ERROR_NOT_IMPLEMENTED = 1,
  FOUNDRY_LOCAL_ERROR_INTERNAL = 2,
  FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT = 3,
  FOUNDRY_LOCAL_ERROR_INVALID_USAGE = 4,
  FOUNDRY_LOCAL_ERROR_OPERATION_CANCELLED = 5,
} flErrorCode;

typedef enum flLogLevel {
  FOUNDRY_LOCAL_LOG_VERBOSE = 0,
  FOUNDRY_LOCAL_LOG_DEBUG = 1,
  FOUNDRY_LOCAL_LOG_INFO = 2,
  FOUNDRY_LOCAL_LOG_WARNING = 3,
  FOUNDRY_LOCAL_LOG_ERROR = 4,
  FOUNDRY_LOCAL_LOG_FATAL = 5
} flLogLevel;

typedef enum flDeviceType {
  FOUNDRY_LOCAL_DEVICE_NOTSET = 0,
  FOUNDRY_LOCAL_DEVICE_CPU = 1,
  FOUNDRY_LOCAL_DEVICE_GPU = 2,
  FOUNDRY_LOCAL_DEVICE_NPU = 3
} flDeviceType;

/// Tensor element data types. Values match ONNX TensorProto.DataType.
typedef enum flTensorDataType {
  FOUNDRY_LOCAL_TENSOR_UNDEFINED = 0,
  FOUNDRY_LOCAL_TENSOR_FLOAT = 1,
  FOUNDRY_LOCAL_TENSOR_UINT8 = 2,
  FOUNDRY_LOCAL_TENSOR_INT8 = 3,
  FOUNDRY_LOCAL_TENSOR_UINT16 = 4,
  FOUNDRY_LOCAL_TENSOR_INT16 = 5,
  FOUNDRY_LOCAL_TENSOR_INT32 = 6,
  FOUNDRY_LOCAL_TENSOR_INT64 = 7,
  FOUNDRY_LOCAL_TENSOR_STRING = 8,
  FOUNDRY_LOCAL_TENSOR_BOOL = 9,
  FOUNDRY_LOCAL_TENSOR_FLOAT16 = 10,
  FOUNDRY_LOCAL_TENSOR_DOUBLE = 11,
  FOUNDRY_LOCAL_TENSOR_UINT32 = 12,
  FOUNDRY_LOCAL_TENSOR_UINT64 = 13,
  FOUNDRY_LOCAL_TENSOR_COMPLEX64 = 14,
  FOUNDRY_LOCAL_TENSOR_COMPLEX128 = 15,
  FOUNDRY_LOCAL_TENSOR_BFLOAT16 = 16,
  FOUNDRY_LOCAL_TENSOR_FLOAT8E4M3FN = 17,
  FOUNDRY_LOCAL_TENSOR_FLOAT8E4M3FNUZ = 18,
  FOUNDRY_LOCAL_TENSOR_FLOAT8E5M2 = 19,
  FOUNDRY_LOCAL_TENSOR_FLOAT8E5M2FNUZ = 20,
  FOUNDRY_LOCAL_TENSOR_UINT4 = 21,
  FOUNDRY_LOCAL_TENSOR_INT4 = 22,
  FOUNDRY_LOCAL_TENSOR_FLOAT4E2M1 = 23,
  FOUNDRY_LOCAL_TENSOR_FLOAT8E8M0 = 24,
} flTensorDataType;

/* -----------------------------------------------------------------------
 * Well-known property key constants for ModelInfo string/int properties.
 * The underlying storage is string-keyed, so arbitrary keys can also be used.
 *
 * _STR suffix = string property (use Info_GetStringProperty)
 * _INT suffix = int64_t property (use Info_GetIntProperty)
 *
 * "optional" means the property may not be present (nullptr / default_value).
 * Bool properties use int64_t: 0 = false, 1 = true.
 * ----------------------------------------------------------------------- */

/* flModelInfo String properties */
#define FOUNDRY_LOCAL_MODEL_PROP_DISPLAY_NAME_STR "display_name"                ///< optional
#define FOUNDRY_LOCAL_MODEL_PROP_MODEL_TYPE_STR "type"                          ///< e.g. "onnx"
#define FOUNDRY_LOCAL_MODEL_PROP_PUBLISHER_STR "publisher"                      ///< optional
#define FOUNDRY_LOCAL_MODEL_PROP_LICENSE_STR "license"                          ///< optional
#define FOUNDRY_LOCAL_MODEL_PROP_LICENSE_DESCRIPTION_STR "license_description"  ///< optional
#define FOUNDRY_LOCAL_MODEL_PROP_TASK_STR "task"                                ///< e.g. "chat-completion", "text-generation"
#define FOUNDRY_LOCAL_MODEL_PROP_MODEL_PROVIDER_STR "model_provider"            ///< e.g. "AzureCatalog"
#define FOUNDRY_LOCAL_MODEL_PROP_MIN_FL_VERSION_STR "min_fl_version"            ///< minimum Foundry Local version required
#define FOUNDRY_LOCAL_MODEL_PROP_PARENT_URI_STR "parent_uri"                    ///< optional
#define FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_START_STR "tool_call_start"          ///< optional tool call start marker token
#define FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_END_STR "tool_call_end"              ///< optional tool call end marker token
#define FOUNDRY_LOCAL_MODEL_PROP_REASONING_START_STR "reasoning_start"          ///< optional reasoning/think start marker token
#define FOUNDRY_LOCAL_MODEL_PROP_REASONING_END_STR "reasoning_end"              ///< optional reasoning/think end marker token

/* flModelInfo Int properties. Comments provide details on the type and expected values. */
#define FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT "supports_tool_calling"  ///< optional bool (not set or -1=unknown, 0=false, 1=true)
#define FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_REASONING_INT "supports_reasoning"        ///< optional bool (not set or -1=unknown, 0=false, 1=true)
#define FOUNDRY_LOCAL_MODEL_PROP_FILESIZE_MB_INT "filesize_mb"                      ///< optional int32_t
#define FOUNDRY_LOCAL_MODEL_PROP_MAX_OUTPUT_TOKENS_INT "max_output_tokens"          ///< optional int32_t
#define FOUNDRY_LOCAL_MODEL_PROP_CREATED_AT_UNIX_INT "created_at_unix"              ///< Unix timestamp. default=0
#define FOUNDRY_LOCAL_MODEL_PROP_IS_TEST_MODEL_INT "is_test_model"                  ///< bool (0=false, 1=true)
#define FOUNDRY_LOCAL_MODEL_PROP_CONTEXT_LENGTH_INT "context_length"                ///< optional int64_t

#define FOUNDRY_LOCAL_MODEL_PROP_INPUT_MODALITIES_STR "input_modalities"   ///< optional, comma-separated
#define FOUNDRY_LOCAL_MODEL_PROP_OUTPUT_MODALITIES_STR "output_modalities" ///< optional, comma-separated
#define FOUNDRY_LOCAL_MODEL_PROP_CAPABILITIES_STR "capabilities"           ///< optional, comma-separated

/* -----------------------------------------------------------------------
 * Well-known parameter keys for Request_SetOptions / Session_SetOptions.
 * Values are JSON-typed strings: floats as "0.7", ints as "256", bools as "true"/"false",
 * strings as their literal value, objects/arrays as JSON text.
 * Arbitrary keys beyond these are allowed — the implementation passes them through.
 * ----------------------------------------------------------------------- */

/* Sampling / generation parameters that can be specified as options for generative sessions. */
#define FOUNDRY_LOCAL_PARAM_TEMPERATURE "temperature"              ///< float [0.0, 2.0]. default model-specific
#define FOUNDRY_LOCAL_PARAM_TOP_P "top_p"                          ///< float [0.0, 1.0]. nucleus sampling
#define FOUNDRY_LOCAL_PARAM_TOP_K "top_k"                          ///< int. top-k sampling
#define FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS "max_output_tokens"  ///< int. max tokens to generate
#define FOUNDRY_LOCAL_PARAM_FREQUENCY_PENALTY "frequency_penalty"  ///< float [-2.0, 2.0]
#define FOUNDRY_LOCAL_PARAM_PRESENCE_PENALTY "presence_penalty"    ///< float [-2.0, 2.0]
#define FOUNDRY_LOCAL_PARAM_SEED "seed"                            ///< int. for reproducible outputs
#define FOUNDRY_LOCAL_PARAM_EARLY_STOPPING "early_stopping"        ///< bool. whether to stop on stop sequence or only at max tokens

/* Request options */
#define FOUNDRY_LOCAL_PARAM_TOOL_CHOICE "tool_choice"  ///< string: "auto", "none", or "required"

// Types supported in request input and response output
// The Item type is opaque and supports various usages. The flItemType determines the operations that can be performed.
// the C++ API provides the logical view of the different usages.
// e.g. Messages, Audio, Tool Call, Tool Call Output, etc. are likely to be required for generative. Types need to
//      support usage via OAI Chat Completions and Responses APIs.
// Tensor and possibly other ONNX types (sparse tensor, optional, opaque) for predictive
//  - TBD if we really need to support the additional ones in FL.
//    are there real use cases or is that a 'use ORT directly' scenario?
//
// We leave some space between the different categories of types to allow for future additions
typedef enum flItemType {
  FOUNDRY_LOCAL_ITEM_UNKNOWN = 0,
  FOUNDRY_LOCAL_ITEM_BYTES = 1,  // Raw bytes with an item type tag.
  FOUNDRY_LOCAL_ITEM_TENSOR = 10,
  FOUNDRY_LOCAL_ITEM_TEXT = 20,
  FOUNDRY_LOCAL_ITEM_MESSAGE = 21,       // role + content string.
  FOUNDRY_LOCAL_ITEM_IMAGE = 25,         // Image input/output. Could be bytes or URI (file, memory address, url, etc.)
  FOUNDRY_LOCAL_ITEM_AUDIO = 30,         // Audio input/output. Could be bytes or URI.
  FOUNDRY_LOCAL_ITEM_TOOL_CALL = 100,    // request to call tool: call id, tool name, arguments
  FOUNDRY_LOCAL_ITEM_TOOL_RESULT = 101,  // response from tool: call id, result
  FOUNDRY_LOCAL_ITEM_QUEUE = 200,        // An item containing an flItemQueue of sub-items. Turtles all the way down.
} flItemType;

typedef enum flTextItemType {
  FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT = 0,      ///< Ordinary text (e.g. assistant response). Default for `flTextData`.
  FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING = 1,    ///< Chain-of-thought / `<think>` content from reasoning models.
  FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON = 2,  ///< Opaque OpenAI REST JSON payload (chat completions / audio
                                                 ///< transcription request or response). Carried as text so the
                                                 ///< runtime can route it without owning a separate item type.
} flTextItemType;

typedef enum flMessageRole {
  FOUNDRY_LOCAL_ROLE_NONE = 0,
  FOUNDRY_LOCAL_ROLE_SYSTEM = 1,
  FOUNDRY_LOCAL_ROLE_USER = 2,
  FOUNDRY_LOCAL_ROLE_ASSISTANT = 3,
  FOUNDRY_LOCAL_ROLE_TOOL = 4,
  FOUNDRY_LOCAL_ROLE_DEVELOPER = 5,
} flMessageRole;

typedef enum flFinishReason {
  FOUNDRY_LOCAL_FINISH_NONE = 0,        ///< Not finished yet (streaming in progress).
  FOUNDRY_LOCAL_FINISH_ERROR = 1,       ///< Generation stopped due to an error.
  FOUNDRY_LOCAL_FINISH_STOP = 2,        ///< Model finished naturally or hit a stop sequence.
  FOUNDRY_LOCAL_FINISH_LENGTH = 3,      ///< Hit max_tokens / max_completion_tokens limit.
  FOUNDRY_LOCAL_FINISH_TOOL_CALLS = 4,  ///< Model is requesting tool calls.
} flFinishReason;

/// Token usage statistics returned with a completed response.
typedef struct flUsage {
  uint32_t version;  ///< Set to FOUNDRY_LOCAL_API_VERSION.
  int64_t prompt_tokens;
  int64_t completion_tokens;
  int64_t total_tokens;
  /* V2 fields go here. */
} flUsage;

/* -----------------------------------------------------------------------
 * Versioned data structs for item types.
 * Each struct carries a `version` field so the implementation can handle
 * older callers gracefully and new fields can be appended without adding
 * new API functions.
 *
 * Callers set `version` to the compile-time *_VERSION constant.
 * - For Set: tells the implementation which fields are populated.
 * - For Get: tells the implementation how many fields the caller expects.
 * ----------------------------------------------------------------------- */

// Forward declarations for per-type deleter typedefs (defined after each struct).
typedef struct flBytesData flBytesData;
typedef struct flTensorData flTensorData;
typedef struct flImageData flImageData;
typedef struct flAudioData flAudioData;

/// Per-type deleter typedefs. Called when the item is destroyed to free data it owns.
/// The deleter receives a const pointer to the data struct (providing direct access to
/// all fields) plus an optional user_data context. The `mutable_data` field is always
/// populated in the struct passed to the deleter, providing a non-const pointer to free.
/// When setting data with a deleter, `mutable_data` must be non-NULL.
typedef void (*flBytesDataDeleter)(const flBytesData* data, void* user_data);
typedef void (*flTensorDataDeleter)(const flTensorData* data, void* user_data);
typedef void (*flImageDataDeleter)(const flImageData* data, void* user_data);
typedef void (*flAudioDataDeleter)(const flAudioData* data, void* user_data);

/// Versioned struct for TEXT item content.
typedef struct flTextData {
  uint32_t version;     ///< Set to FOUNDRY_LOCAL_API_VERSION.
  const char* text;     ///< UTF-8 text. Borrowed by caller; copied on Set. Pointer into item storage on Get.
  flTextItemType type;  ///< Text type tag. Defaults to FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT.
  /* V2 fields go here. */
} flTextData;

/// Versioned struct for raw bytes data.
struct flBytesData {
  uint32_t version;              ///< Set to FOUNDRY_LOCAL_API_VERSION.
  flItemType item_type;          ///< The type of item these bytes are from, e.g. AUDIO
  const void* data;              ///< Read-only data pointer. Always populated on Get.
  void* mutable_data;            ///< Writable data pointer. NULL for read-only data. NULL on Get.
  size_t data_size;              ///< Byte count.
  flBytesDataDeleter deleter;    ///< Optional. Called on item destruction to free owned data.
  void* deleter_user_data;       ///< Context for deleter. Ignored if deleter is NULL.
  /* V2 fields go here. */
};

/// Versioned struct for tensor data.
struct flTensorData {
  uint32_t version;              ///< Set to FOUNDRY_LOCAL_API_VERSION.
  flTensorDataType data_type;    ///< Element data type.
  const void* data;              ///< Read-only data pointer. Always populated on Get.
  void* mutable_data;            ///< Writable data pointer. NULL for read-only data. NULL on Get.
  const int64_t* shape;          ///< Array of dimension sizes. Length is `rank`.
  size_t rank;                   ///< Number of dimensions.
  flTensorDataDeleter deleter;   ///< Optional. Called on item destruction to free owned data.
  void* deleter_user_data;       ///< Context for deleter. Ignored if deleter is NULL.
  /* V2 fields go here. */
};

/// Versioned struct for message data.
///
/// Message content is a typed array of part items. Each entry in `content_items`
/// MUST be a `flItem*` whose `flItemType` is one of:
///   - FOUNDRY_LOCAL_ITEM_TEXT
///   - FOUNDRY_LOCAL_ITEM_IMAGE
///   - FOUNDRY_LOCAL_ITEM_AUDIO
/// Other item types are rejected with FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT.
///
/// Single-text messages are expressed as a one-element array containing a TEXT item.
/// Language bindings provide single-string convenience overloads.
typedef struct flMessageData {
  uint32_t version;                    ///< Set to FOUNDRY_LOCAL_API_VERSION.
  flMessageRole role;                  ///< Message role (FOUNDRY_LOCAL_ROLE_NONE to omit).
  const flItem* const* content_items;  ///< Array of content part items (TEXT/IMAGE/AUDIO).
  size_t content_items_count;          ///< Number of entries in `content_items`.
  const char* name;                    ///< Optional participant name within a role. NULL to omit.
  /* V2 fields go here. */
} flMessageData;

/// Versioned struct for image data.
/// Set `data`+`data_size` for byte-based images, or `uri` for URI-based images.
struct flImageData {
  uint32_t version;              ///< Set to FOUNDRY_LOCAL_API_VERSION.
  const void* data;              ///< Read-only data pointer. Always populated on Get.
  void* mutable_data;            ///< Writable data pointer. NULL for read-only data. NULL on Get.
  size_t data_size;              ///< Byte count. 0 for URI-based images.
  const char* format;            ///< Image format, e.g. "png". May be NULL for URI-based images.
  const char* uri;               ///< File path, URL, etc. NULL for byte-based images.
  flImageDataDeleter deleter;    ///< Optional. Called on item destruction to free owned data.
  void* deleter_user_data;       ///< Context for deleter. Ignored if deleter is NULL.
  /* V2 fields go here. */
};

struct flAudioData {
  uint32_t version;              ///< Set to FOUNDRY_LOCAL_API_VERSION.
  const void* data;              ///< Read-only data pointer. Always populated on Get.
  void* mutable_data;            ///< Writable data pointer. NULL for read-only data. NULL on Get.
  size_t data_size;              ///< Byte count. 0 for URI-based audio.
  const char* format;            ///< Audio format, e.g. "mp3", "pcm". May be NULL for URI-based audio.
  const char* uri;               ///< File path, URL, etc. NULL for byte-based audio.
  int sample_rate;               ///< Sample rate in Hz (e.g. 16000). 0 = unspecified.
  int channels;                  ///< Channel count: 1 = mono, 2 = stereo. 0 = unspecified.
  flAudioDataDeleter deleter;    ///< Optional. Called on item destruction to free owned data.
  void* deleter_user_data;       ///< Context for deleter. Ignored if deleter is NULL.
};

/// Versioned struct for tool call data.
typedef struct flToolCallData {
  uint32_t version;       ///< Set to FOUNDRY_LOCAL_API_VERSION.
  const char* call_id;    ///< Tool call identifier.
  const char* name;       ///< Tool name.
  const char* arguments;  ///< JSON-encoded arguments.
  /* V2 fields go here. */
} flToolCallData;

/// Versioned struct for tool result data.
typedef struct flToolResultData {
  uint32_t version;     ///< Set to FOUNDRY_LOCAL_API_VERSION.
  const char* call_id;  ///< Tool call identifier this result is for.
  const char* result;   ///< Result content.
  /* V2 fields go here. */
} flToolResultData;

/// Versioned struct that we pass to a callback during Session::ProcessRequest.
/// Guarantees ordering and synchronization via the flItemQueue.
typedef struct flStreamingCallbackData {
  uint32_t version;         ///< Set to FOUNDRY_LOCAL_API_VERSION;
  flItemQueue* item_queue;  // One item is added to the queue for each callback.
  // TBD if we need output_index of content_index values at the Session.Run callback level or whether that is
  // handled above when converting to chat completions or responses api events.
} flStreamingCallbackData;

typedef struct flToolDefinition {
  uint32_t version;         ///< Set to FOUNDRY_LOCAL_API_VERSION;
  const char* name;         ///< Tool name.
  const char* description;  ///< Tool description for model context.
  const char* json_schema;  ///< JSON schema defining the tool's arguments.
  /* V2 fields go here. */
} flToolDefinition;

/* -----------------------------------------------------------------------
 * Callback types
 * ----------------------------------------------------------------------- */

/** Progress callback for model downloads. value is 0.0 to 100.0.
 *  Return 0 to continue, non-zero to cancel the operation reporting progress. */
typedef int (*flProgressCallback)(float value, void* user_data);

/** Streaming response event callback. Return 0 to continue, non-zero to cancel. */
typedef int (*flStreamingCallback)(flStreamingCallbackData event, void* user_data);

/** EP download progress callback. ep_name identifies the EP being processed.
 *  value is 0.0 to 100.0.
 *  Return 0 to continue, non-zero to cancel the download. */
typedef int (*flEpProgressCallback)(_In_z_ const char* ep_name, float value, _In_opt_ void* user_data);

/* -----------------------------------------------------------------------
 * API function tables
 *
 * The library exposes its functionality through versioned structs of
 * function pointers. Only three symbols are exported from the shared
 * library; everything else is accessed through these tables.
 *
 * To maintain ABI compatibility, append new entries at the end of each
 * struct. Never remove or reorder existing entries.
 * ----------------------------------------------------------------------- */

// Forward declarations for sub-API structs defined after flApi.
typedef struct flCatalogApi flCatalogApi;
typedef struct flConfigurationApi flConfigurationApi;
typedef struct flItemApi flItemApi;
typedef struct flInferenceApi flInferenceApi;
typedef struct flModelApi flModelApi;

/* --- Root API ---------------------------------------------------------- */
typedef struct flApi {
  /* Status */
  FL_API_STATUS(Status_Create, flErrorCode error_code, _In_ const char* error_msg);
  FL_TYPE_RELEASE(Status);
  flErrorCode FL_API_T(Status_GetErrorCode, _In_ const flStatus* status);
  const char* FL_API_T(Status_GetErrorMessage, _In_ const flStatus* status);

  /* Manager lifecycle */
  FL_API_STATUS(Manager_Create, _In_ const flConfiguration* config, _Outptr_ flManager** out_manager);
  FL_TYPE_RELEASE(Manager);

  FL_API_STATUS(Manager_GetCatalog, _In_ const flManager* manager, _Outptr_ flCatalog** out_catalog);
  FL_API_STATUS(Manager_WebServiceStart, _In_ flManager* manager);
  // Get the bound service urls. Error if web service is not running.
  FL_API_STATUS(Manager_WebServiceUrls, _In_ const flManager* manager,
                _Out_ const char* const** out_urls, _Out_ size_t* out_num_urls);
  FL_API_STATUS(Manager_WebServiceStop, _In_ flManager* manager);

  /* Sub-API accessors. Returned pointers are valid for the lifetime of the library. */
  const flCatalogApi* FL_API_T(GetCatalogApi, void);
  const flConfigurationApi* FL_API_T(GetConfigurationApi, void);
  const flItemApi* FL_API_T(GetItemApi, void);
  const flInferenceApi* FL_API_T(GetInferenceApi, void);
  const flModelApi* FL_API_T(GetModelApi, void);

  /* KeyValuePairs */
  void FL_API_T(CreateKeyValuePairs, _Outptr_ flKeyValuePairs** out);
  // add/replace
  void FL_API_T(AddKeyValuePair, _In_ flKeyValuePairs* kvps, _In_ const char* key, _In_ const char* value);
  // get. returns nullptr for not found.
  const char* FL_API_T(GetKeyValue, _In_ const flKeyValuePairs* kvps, _In_ const char* key);
  void FL_API_T(GetKeyValuePairs, _In_ const flKeyValuePairs* kvps,
                _Outptr_ const char* const** keys, _Outptr_ const char* const** values,
                _Out_ size_t* num_entries);
  void FL_API_T(RemoveKeyValuePair, _In_ flKeyValuePairs* kvps, _In_ const char* key);
  FL_TYPE_RELEASE(KeyValuePairs);

  // flModelList is opaque so we can do some optimizations, especially when returning the full list of models
  // in the catalog. we can cache that info in the implementation and avoid creating an array of pointers every time.
  // Cost is iteration is slightly harder, but we can simplify that at the C++ level.
  FL_TYPE_RELEASE(ModelList);
  size_t FL_API_T(ModelList_Size, _In_ const flModelList* models);
  flModel* FL_API_T(ModelList_GetAt, _In_ const flModelList* models, size_t idx);

  /* EP detection */

  /// Get discoverable execution providers and their registration status.
  /// Returns parallel arrays of EP names and registration flags, plus count.
  /// Returned data is owned by the Manager — valid until the next call to
  /// Manager_GetDiscoverableEps or Manager destruction.
  FL_API_STATUS(Manager_GetDiscoverableEps, _In_ const flManager* manager,
                _Out_ const char* const** out_names,
                _Out_ const int** out_is_registered,
                _Out_ size_t* out_count);

  /// Download and register execution providers. Blocking.
  /// ep_names is an optional array of EP names to download. NULL = all discoverable EPs.
  /// num_ep_names is the number of entries in ep_names. Ignored when ep_names is NULL.
  /// Returns nullptr on full success, non-null flStatus* when at least one EP failed.
  FL_API_STATUS(Manager_DownloadAndRegisterEps, _In_ flManager* manager,
                _In_opt_ const char* const* ep_names,
                size_t num_ep_names,
                _In_opt_ flEpProgressCallback callback,
                _In_opt_ void* user_data);

  /// Whether an EP download/registration operation is currently in progress.
  bool FL_API_T(Manager_IsEpDownloadInProgress, _In_ const flManager* manager);

  /// Begin graceful shutdown. Safe to call from any thread. Idempotent.
  FL_API_STATUS(Manager_Shutdown, _In_ flManager* manager);

  /// Check if Shutdown has been called.
  bool FL_API_T(Manager_IsShutdownRequested, _In_ const flManager* manager);

  // End V1
  /* Append new function pointers at the end for future versions and add marker for the end of each version */
} flApi;

struct flItemApi {
  /* Item lifecycle */
  FL_API_STATUS(Create, flItemType type, _Outptr_ flItem** out_item);
  FL_TYPE_RELEASE(Item);

  /// Get the type of an item.
  flItemType FL_API_T(GetType, _In_ const flItem* item);

  /* Setters — must be valid for the flItemType the item was created with, otherwise returns an error.
   */

  /// Set raw bytes data from a versioned struct.
  FL_API_STATUS(SetBytes, _In_ flItem* item, _In_ const flBytesData* bytes);

  /// Set TENSOR data from a versioned struct.
  FL_API_STATUS(SetTensor, _In_ flItem* item, _In_ const flTensorData* tensor);

  /// Set TEXT item content from a versioned struct (text + subtype). Input is copied.
  /// `text_data->text` must be non-NULL.
  FL_API_STATUS(SetText, _In_ flItem* item, _In_ const flTextData* text_data);

  /// Set content for a MESSAGE item from a versioned struct.
  ///
  /// Lifetime: the part flItem*s referenced by `message->content_items` are
  /// borrowed by the MESSAGE item. They must remain valid as long as the
  /// MESSAGE item is used in its current form. Once the runtime retains the
  /// MESSAGE (e.g. into chat history), it deep-copies every part — including
  /// duplicating any IMAGE/AUDIO byte buffers — so the cached message is
  /// fully independent of the caller's storage.
  FL_API_STATUS(SetMessage, _In_ flItem* item, _In_ const flMessageData* message);

  /// Set IMAGE data from a versioned struct. Set data+data_size for bytes, or uri for URI-based.
  FL_API_STATUS(SetImage, _In_ flItem* item, _In_ const flImageData* image);

  /// Set AUDIO data from a versioned struct. Set data+data_size for bytes, or uri for URI-based.
  FL_API_STATUS(SetAudio, _In_ flItem* item, _In_ const flAudioData* audio);

  /// Set content for a TOOL_CALL item from a versioned struct.
  FL_API_STATUS(SetToolCall, _In_ flItem* item, _In_ const flToolCallData* tool_call);
  /// Set content for a TOOL_RESULT item from a versioned struct.
  FL_API_STATUS(SetToolResult, _In_ flItem* item, _In_ const flToolResultData* tool_result);

  /* Getters — must be valid for the flItemType the item was created with, otherwise returns an error. */

  /// Get raw bytes data into a versioned struct. Returned pointer is valid until the item is released.
  FL_API_STATUS(GetBytes, _In_ const flItem* item, _Out_ flBytesData* out_bytes);
  /// Get TEXT item content into a versioned struct. Returned pointer is valid until the item is released.
  FL_API_STATUS(GetText, _In_ const flItem* item, _Out_ flTextData* out_text_data);
  /// Get content of a MESSAGE item into a versioned struct.
  FL_API_STATUS(GetMessage, _In_ const flItem* item, _Out_ flMessageData* out_message);
  /// Get tensor data into a versioned struct. Returned pointers are valid until the item is released.
  FL_API_STATUS(GetTensor, _In_ const flItem* item, _Out_ flTensorData* out_tensor);
  /// Get image data into a versioned struct. Check data vs uri to determine the image source type.
  FL_API_STATUS(GetImage, _In_ const flItem* item, _Out_ flImageData* out_image);
  /// Get audio data into a versioned struct. Check data vs uri to determine the audio source type.
  FL_API_STATUS(GetAudio, _In_ const flItem* item, _Out_ flAudioData* out_audio);

  /// Get content of a TOOL_CALL item into a versioned struct.
  FL_API_STATUS(GetToolCall, _In_ const flItem* item, _Out_ flToolCallData* out_tool_call);
  /// Get content of a TOOL_RESULT item into a versioned struct.
  FL_API_STATUS(GetToolResult, _In_ const flItem* item, _Out_ flToolResultData* out_tool_result);

  /// Get metadata from the item (read-only).
  FL_API_STATUS(GetMetadata, _In_ const flItem* item, _Outptr_ const flKeyValuePairs** out_metadata);

  /// Get mutable metadata from the item. Returned pairs are owned by the item.
  FL_API_STATUS(GetMutableMetadata, _In_ flItem* item, _Outptr_ flKeyValuePairs** out_metadata);

  /// Get the queue from a QUEUE item. Returned queue is owned by the item — do not release it.
  FL_API_STATUS(GetQueue, _In_ flItem* item, _Outptr_ flItemQueue** out_queue);

  /// ItemQueue
  FL_API_STATUS(ItemQueue_Create, _Outptr_ flItemQueue** out_queue);
  FL_TYPE_RELEASE(ItemQueue);
  FL_API_STATUS(ItemQueue_Push, _In_ flItemQueue* queue, _In_ flItem* item);
  // Remove item from front of queue and return it. Returns false if queue is empty.
  // Caller takes ownership of the item and must call Item_Release when done
  bool FL_API_T(ItemQueue_TryPop, _In_ flItemQueue* queue, _Out_opt_ flItem** out_item);
  size_t FL_API_T(ItemQueue_Size, _In_ const flItemQueue* queue);

  void FL_API_T(ItemQueue_MarkFinished, _In_ flItemQueue* queue);  ///< Producer is done
  bool FL_API_T(ItemQueue_IsFinished, _In_ const flItemQueue* queue);

  // End V1
};

struct flInferenceApi {
  /* Request */
  FL_API_STATUS(Request_Create, _Outptr_ flRequest** out_request);
  FL_TYPE_RELEASE(Request);
  /// Add an item to the request. Request takes ownership of the item if take_ownership is true — caller must keep valid
  /// until the request is finished.
  FL_API_STATUS(Request_AddItem, _In_ flRequest* request, _In_ flItem* item, _In_ bool take_ownership);
  size_t FL_API_T(Request_GetItemCount, _In_ const flRequest* request);
  FL_API_STATUS(Request_GetItem, _In_ const flRequest* request, size_t idx, _Outptr_ const flItem** out_item);
  /// Set inference options from key/value pairs. Use FOUNDRY_LOCAL_PARAM_* constants for well-known keys.
  /// Values are string representations; the implementation parses them for the appropriate type.
  /// The request copies the data — the caller may release the pairs after this call.
  FL_API_STATUS(Request_SetOptions, _In_ flRequest* request, _In_ const flKeyValuePairs* options);
  /// Cancel an in-progress request.
  FL_API_STATUS(Request_Cancel, _In_ flRequest* request);

  /* Response */
  FL_API_STATUS(Response_Create, _Outptr_ flResponse** out_response);
  FL_TYPE_RELEASE(Response);
  /// Get output items from the response after Session_Run completes.
  size_t FL_API_T(Response_GetItemCount, _In_ const flResponse* response);
  FL_API_STATUS(Response_GetItem, _In_ const flResponse* response, size_t idx, _Outptr_ const flItem** out_item);
  /// Get the finish reason after Session_Run completes.
  flFinishReason FL_API_T(Response_GetFinishReason, _In_ const flResponse* response);
  /// Get token usage statistics after Session_Run completes.
  FL_API_STATUS(Response_GetUsage, _In_ const flResponse* response, _Out_ flUsage* out_usage);

  /* Session */
  FL_API_STATUS(Session_Create, _In_ const flModel* model, _Outptr_ flSession** out_session);
  FL_TYPE_RELEASE(Session);

  /// Set streaming callback for the session. Used for subsequent calls to Session_Run. Provide nullptr to unset.
  FL_API_STATUS(Session_SetStreamingCallback, _In_ flSession* session,
                _In_ flStreamingCallback callback, _In_opt_ void* user_data);

  /// Set session-level inference options from key/value pairs. Use FOUNDRY_LOCAL_PARAM_* constants for well-known keys.
  /// Session options apply to all subsequent ProcessRequest calls unless overridden per-request.
  /// The session copies the data — the caller may release the pairs after this call.
  FL_API_STATUS(Session_SetOptions, _In_ flSession* session, _In_ const flKeyValuePairs* options);

  /// Process a request for the session.
  /// Provide a pre-allocated response is optional. Use Response_Create.
  /// Intended usage is for pre-allocated on-device outputs.
  /// Response will be allocated otherwise. Caller owns and must call Response_Release.
  /// For streaming, set the event callback on the session before calling this.
  FL_API_STATUS(Session_ProcessRequest, _In_ flSession* session, _In_ const flRequest* request,
                _Inout_ flResponse** response);

  /* Chat-session features — these operate on the conversation state maintained
     by chat sessions. For non-chat session types, tool definitions are ignored,
     turn count returns 0, and UndoTurns returns an error. */

  /// Add a tool definition to the session. The session copies the data.
  FL_API_STATUS(Session_AddToolDefinition, _In_ flSession* session, _In_ const flToolDefinition* tool_def);

  /// Get the number of completed turns in the session.
  size_t FL_API_T(Session_GetTurnCount, _In_ const flSession* session);

  /// Undo the last `count` turns. Rewinds the generator and removes the turns' messages from history.
  /// If all turns are undone, the cached generator is destroyed.
  FL_API_STATUS(Session_UndoTurns, _In_ flSession* session, size_t count);

  // End V1
};

/* --- Configuration API ------------------------------------------------- */
struct flConfigurationApi {
  /// Create a configuration. app_name is required and must not be null/empty.
  FL_API_STATUS(Create, _In_ const char* app_name, _Outptr_ flConfiguration** out_config);
  FL_TYPE_RELEASE(Configuration);
  /// Optional. Default log level. Defaults to Warning.
  FL_API_STATUS(SetDefaultLogLevel, _In_ flConfiguration* config, flLogLevel level);
  /// Optional. Directory for app data. Defaults to ~/.<app_name>.
  /// `{home}` can be used as a placeholder for the user's home directory.
  FL_API_STATUS(SetAppDataDir, _In_ flConfiguration* config, _In_ const char* dir);
  /// Optional. Directory for log files. Defaults to <app_data_dir>/logs.
  FL_API_STATUS(SetLogsDir, _In_ flConfiguration* config, _In_ const char* dir);
  /// Optional. Directory for cached models. Defaults to <app_data_dir>/cache/models.
  FL_API_STATUS(SetModelCacheDir, _In_ flConfiguration* config, _In_ const char* dir);
  /// Optional. Directory containing ORT runtime libraries (onnxruntime.dll, onnxruntime-genai.dll).
  /// Only needed when ORT DLLs are not co-located with foundry_local.dll.
  /// When not set, the default DLL search order is used (co-located DLLs are found first).
  FL_API_STATUS(SetRuntimeLibraryPath, _In_ flConfiguration* config, _In_ const char* dir);
  /// Optional. Add a catalog URL. Defaults to the Azure Foundry Local Catalog if none added.
  /// Multiple catalogs can be added. Catalogs priority is determined by the order they were added.
  /// @param filter_override Optional filter string for this catalog. Pass NULL for no override.
  FL_API_STATUS(AddCatalogUrl, _In_ flConfiguration* config, _In_ const char* url,
                _In_opt_ const char* filter_override);
  /// Optional. Azure region for the model registry download endpoint
  /// (https://{region}.api.azureml.ms/modelregistry/...). Resolves a model's
  /// asset_id to a downloadable blob storage URL. Defaults to "eastus" when not set.
  FL_API_STATUS(SetCatalogRegion, _In_ flConfiguration* config, _In_ const char* region);
  /// Optional. Add a web service endpoint to bind to.
  /// Defaults to "http://127.0.0.1:0" (ephemeral port) if none added.
  /// Multiple endpoints can be added. Service will bind to all provided endpoints on StartService.
  FL_API_STATUS(AddWebServiceEndpoint, _In_ flConfiguration* config, _In_ const char* url);

  /// Optional. URL of an external Foundry Local service for client-only mode.
  /// When set, the catalog reads only from the local disk cache and local-only
  /// operations (StartWebService, session creation) return errors.
  FL_API_STATUS(SetExternalServiceUrl, _In_ flConfiguration* config, _In_ const char* url);

  /// Optional. Set additional/undocumented options as key/value pairs.
  /// These are passed through to the core implementation. The configuration copies the data.
  FL_API_STATUS(SetAdditionalOptions, _In_ flConfiguration* config, _In_ const flKeyValuePairs* options);

  // End V1
};

/* --- Catalog API ------------------------------------------------------- */
struct flCatalogApi {
  /// Get the catalog name. For Azure catalogs this is the catalog URI.
  /// The returned string is owned by the catalog — do not free it.
  FL_API_STATUS(GetName, _In_ const flCatalog* catalog, _Out_ const char** out_name);

  // Catalog owns model list. Cached for efficiency.
  // Models are mutable for load/unload/remove operations. Model info is immutable though.
  FL_API_STATUS(GetModels, _In_ const flCatalog* catalog, _Outptr_ flModelList** out_models);
  FL_API_STATUS(GetModel, _In_ const flCatalog* catalog, _In_ const char* alias,
                _Outptr_ flModel** out_model);

  /// Lookup a specific model variant by its unique model ID.
  FL_API_STATUS(GetModelVariant, _In_ const flCatalog* catalog, _In_ const char* model_id,
                _Outptr_ flModel** out_model);

  /// Get the latest version of a model from the catalog.
  FL_API_STATUS(GetLatestVersion, _In_ const flCatalog* catalog, _In_ const flModel* model,
                _Outptr_ flModel** out_model);

  FL_API_STATUS(GetCachedModels, _In_ const flCatalog* catalog, _Outptr_ flModelList** out_models);
  FL_API_STATUS(GetLoadedModels, _In_ const flCatalog* catalog, _Outptr_ flModelList** out_models);

  // End V1
};

/* --- Model API --------------------------------------------------------- */
struct flModelApi {
  /* ModelInfo accessors. flModelInfo is owned by the Model — do not release. */
  FL_API_STATUS(GetInfo, _In_ const flModel* model, _Outptr_ const flModelInfo** out_info);

  /// Get the model input and output information.
  /// Returns arrays of const item pointers describing expected inputs and outputs.
  /// The returned arrays and items are owned by the model — do not release them.
  FL_API_STATUS(GetInputOutputInfo, _In_ const flModel* model,
                _Outptr_ const flItem* const** out_inputs, _Out_ size_t* out_num_inputs,
                _Outptr_ const flItem* const** out_outputs, _Out_ size_t* out_num_outputs);

  /* Model handle operations. Catalog owns Model instances. */
  FL_API_STATUS(IsCached, _In_ const flModel* model, _Out_ int* out_cached);
  FL_API_STATUS(GetPath, _In_ const flModel* model, _Out_ const char** out_path);
  FL_API_STATUS(Download, _In_ flModel* model, _In_opt_ flProgressCallback callback,
                _In_opt_ void* user_data);

  FL_API_STATUS(IsLoaded, _In_ const flModel* model, _Out_ int* out_loaded);
  FL_API_STATUS(Load, _In_ flModel* model);
  FL_API_STATUS(Unload, _In_ flModel* model);
  FL_API_STATUS(RemoveFromCache, _In_ flModel* model);

  /* Variant support. A model may have multiple device-optimized variants.
     A leaf variant reports itself as its only variant.
     Operations like IsCached/IsLoaded/GetInfo delegate to the selected variant. */

  /// Get the variants of this model. A leaf variant returns a list containing itself.
  /// Caller owns the returned model list and must release it via ModelList_Release.
  FL_API_STATUS(GetVariants, _In_ const flModel* model, _Outptr_ flModelList** out_variants);

  /// Select a specific variant. Error if variant is not in this model's variant list.
  /// Throws if the flModel is for a variant (came from Catalog.GetModelVariant or Model.GetVariants) as that
  /// represent a single variant and the user is likely confused.
  FL_API_STATUS(SelectVariant, _In_ flModel* model, _In_ const flModel* variant);

  // Core identity fields
  const char* FL_API_T(Info_GetId, _In_ const flModelInfo* info);
  const char* FL_API_T(Info_GetName, _In_ const flModelInfo* info);
  int FL_API_T(Info_GetVersion, _In_ const flModelInfo* info);
  const char* FL_API_T(Info_GetAlias, _In_ const flModelInfo* info);
  const char* FL_API_T(Info_GetUri, _In_ const flModelInfo* info);

  /// Get the device type the model is optimized for.
  flDeviceType FL_API_T(Info_GetDeviceType, _In_ const flModelInfo* info);
  /// Returns nullptr if not set.
  const char* FL_API_T(Info_GetExecutionProvider, _In_ const flModelInfo* info);
  /// Task the model performs. "chat-completion", "automatic-speech-recognition", "vision-language-chat"
  const char* FL_API_T(Info_GetTask, _In_ const flModelInfo* info);

  /// Get prompt templates as key/value pairs. Returns nullptr if none.
  const flKeyValuePairs* FL_API_T(Info_GetPromptTemplates, _In_ const flModelInfo* info);
  /// Get model settings as key/value pairs. Value may be null for a given key.
  const flKeyValuePairs* FL_API_T(Info_GetModelSettings, _In_ const flModelInfo* info);

  /// Get a string property by key. Returns nullptr if not set.
  /// Use FL_MODEL_PROP_* constants for well-known keys, or any arbitrary string key.
  const char* FL_API_T(Info_GetStringProperty, _In_ const flModelInfo* info, _In_ const char* key);
  /// Get an int property by key. Returns default_value if not set.
  /// Use FL_MODEL_PROP_* constants for well-known keys, or any arbitrary string key.
  int64_t FL_API_T(Info_GetIntProperty, _In_ const flModelInfo* info, _In_ const char* key, int64_t default_value);

  // End V1
};

#ifdef __cplusplus
} /* extern "C" */
#endif
