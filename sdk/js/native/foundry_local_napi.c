/**
 * Node-API C addon for the Foundry Local JS SDK.
 *
 * Replaces the koffi FFI bridge with a lightweight native addon that
 * dynamically loads the FoundryLocalCore shared library at runtime and
 * exposes three JavaScript-callable functions:
 *
 *   loadLibrary(corePath, depPaths?)  – load native libs, resolve symbols
 *   executeCommand(cmd, dataJson)     – synchronous command execution
 *   executeCommandWithBinary(cmd, dataJson, binaryBuf) – with binary payload
 *   executeCommandStreaming(cmd, dataJson, callback) – async + streaming
 */

#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Platform-specific dynamic loading ─────────────────────────────────── */

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  typedef HMODULE lib_handle_t;
  #define LIB_OPEN(path)       LoadLibraryA(path)
  #define LIB_SYM(handle, sym) GetProcAddress(handle, sym)
  #define LIB_CLOSE(handle)    FreeLibrary(handle)
#else
  #include <dlfcn.h>
  typedef void* lib_handle_t;
  #define LIB_OPEN(path)       dlopen(path, RTLD_NOW | RTLD_LOCAL)
  #define LIB_SYM(handle, sym) dlsym(handle, sym)
  #define LIB_CLOSE(handle)    dlclose(handle)
#endif

/* ── Native core structs (must match C# / Rust definitions) ───────────── */

typedef struct {
    const char* Command;
    int32_t     CommandLength;
    const char* Data;
    int32_t     DataLength;
} RequestBuffer;

typedef struct {
    void*   Data;
    int32_t DataLength;
    void*   Error;
    int32_t ErrorLength;
} ResponseBuffer;

typedef struct {
    const char* Command;
    int32_t     CommandLength;
    const char* Data;
    int32_t     DataLength;
    const void* BinaryData;
    int32_t     BinaryDataLength;
} StreamingRequestBuffer;

typedef int32_t (*CallbackFn)(const void* data, int32_t length, void* userData);

/* ── Native function pointer types ────────────────────────────────────── */

typedef void (*ExecuteCommandFn)(
    const RequestBuffer* request,
    ResponseBuffer* response
);

typedef void (*ExecuteCommandWithCallbackFn)(
    const RequestBuffer* request,
    ResponseBuffer* response,
    CallbackFn callback,
    void* userData
);

typedef void (*ExecuteCommandWithBinaryFn)(
    const StreamingRequestBuffer* request,
    ResponseBuffer* response
);

/* ── Module state ─────────────────────────────────────────────────────── */

static lib_handle_t g_core_lib = NULL;
static lib_handle_t* g_dep_libs = NULL;
static size_t g_dep_lib_count = 0;

static ExecuteCommandFn g_execute_command = NULL;
static ExecuteCommandWithCallbackFn g_execute_command_with_callback = NULL;
static ExecuteCommandWithBinaryFn g_execute_command_with_binary = NULL;

/* ── Platform-specific memory deallocation ────────────────────────────── */

/*
 * The .NET native core allocates response buffers with Marshal.AllocHGlobal:
 *   - Unix:    malloc  → free with free()
 *   - Windows: LocalAlloc → free with LocalFree()
 */
static void free_native_buffer(void* ptr) {
    if (!ptr) return;
#ifdef _WIN32
    LocalFree(ptr);
#else
    free(ptr);
#endif
}

/* ── Helper: throw JS error from napi_status ──────────────────────────── */

#define NAPI_CALL(env, call)                                      \
    do {                                                          \
        napi_status _status = (call);                             \
        if (_status != napi_ok) {                                 \
            const napi_extended_error_info* _err_info = NULL;     \
            napi_get_last_error_info((env), &_err_info);          \
            const char* _msg = (_err_info && _err_info->error_message) \
                ? _err_info->error_message                        \
                : "Unknown N-API error";                          \
            napi_throw_error((env), NULL, _msg);                  \
            return NULL;                                          \
        }                                                         \
    } while (0)

/* ── Helper: extract response and free native buffers ─────────────────── */

static napi_value handle_response(napi_env env, const char* command,
                                  ResponseBuffer* res) {
    napi_value result;

    if (res->Error && res->ErrorLength > 0) {
        char* msg = (char*)malloc(res->ErrorLength + 64);
        if (msg) {
            snprintf(msg, res->ErrorLength + 64, "Command '%s' failed: %.*s",
                     command, res->ErrorLength, (const char*)res->Error);
            napi_throw_error(env, NULL, msg);
            free(msg);
        } else {
            napi_throw_error(env, NULL, "Command failed (out of memory for error)");
        }
        free_native_buffer(res->Data);
        free_native_buffer(res->Error);
        return NULL;
    }

    if (res->Data && res->DataLength > 0) {
        NAPI_CALL(env, napi_create_string_utf8(env, (const char*)res->Data,
                                                res->DataLength, &result));
    } else {
        NAPI_CALL(env, napi_create_string_utf8(env, "", 0, &result));
    }

    free_native_buffer(res->Data);
    free_native_buffer(res->Error);
    return result;
}

/* ── loadLibrary(corePath, depPaths?) ─────────────────────────────────── */

static napi_value napi_load_library(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value argv[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    if (argc < 1) {
        napi_throw_error(env, NULL, "loadLibrary requires at least 1 argument (corePath)");
        return NULL;
    }

    /* Close previously loaded libraries if any */
    if (g_core_lib) {
        LIB_CLOSE(g_core_lib);
        g_core_lib = NULL;
    }
    for (size_t i = 0; i < g_dep_lib_count; i++) {
        if (g_dep_libs[i]) LIB_CLOSE(g_dep_libs[i]);
    }
    free(g_dep_libs);
    g_dep_libs = NULL;
    g_dep_lib_count = 0;
    g_execute_command = NULL;
    g_execute_command_with_callback = NULL;
    g_execute_command_with_binary = NULL;

    /* Load dependency libraries first (e.g., onnxruntime on Windows) */
    if (argc >= 2) {
        napi_valuetype vt;
        NAPI_CALL(env, napi_typeof(env, argv[1], &vt));

        if (vt != napi_undefined && vt != napi_null) {
            bool is_array = false;
            NAPI_CALL(env, napi_is_array(env, argv[1], &is_array));
            if (!is_array) {
                napi_throw_type_error(env, NULL, "depPaths must be an array of strings");
                return NULL;
            }

            uint32_t dep_count = 0;
            NAPI_CALL(env, napi_get_array_length(env, argv[1], &dep_count));

            if (dep_count > 0) {
                g_dep_libs = (lib_handle_t*)calloc(dep_count, sizeof(lib_handle_t));
                if (!g_dep_libs) {
                    napi_throw_error(env, NULL, "Out of memory");
                    return NULL;
                }
                g_dep_lib_count = dep_count;

                for (uint32_t i = 0; i < dep_count; i++) {
                    napi_value elem;
                    NAPI_CALL(env, napi_get_element(env, argv[1], i, &elem));

                    size_t len = 0;
                    NAPI_CALL(env, napi_get_value_string_utf8(env, elem, NULL, 0, &len));
                    char* dep_path = (char*)malloc(len + 1);
                    if (!dep_path) {
                        napi_throw_error(env, NULL, "Out of memory");
                        return NULL;
                    }
                    NAPI_CALL(env, napi_get_value_string_utf8(env, elem, dep_path, len + 1, &len));

                    g_dep_libs[i] = LIB_OPEN(dep_path);
                    if (!g_dep_libs[i]) {
                        char err_msg[512];
                        snprintf(err_msg, sizeof(err_msg),
                                 "Failed to load dependency library: %s", dep_path);
                        free(dep_path);
                        napi_throw_error(env, NULL, err_msg);
                        return NULL;
                    }
                    free(dep_path);
                }
            }
        }
    }

    /* Load the core library */
    size_t core_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], NULL, 0, &core_len));
    char* core_path = (char*)malloc(core_len + 1);
    if (!core_path) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], core_path, core_len + 1, &core_len));

    g_core_lib = LIB_OPEN(core_path);
    if (!g_core_lib) {
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg),
                 "Failed to load core library: %s", core_path);
        free(core_path);
        napi_throw_error(env, NULL, err_msg);
        return NULL;
    }
    free(core_path);

    /* Resolve function pointers */
    g_execute_command = (ExecuteCommandFn)LIB_SYM(g_core_lib, "execute_command");
    if (!g_execute_command) {
        napi_throw_error(env, NULL, "Failed to resolve 'execute_command' symbol");
        return NULL;
    }

    g_execute_command_with_callback = (ExecuteCommandWithCallbackFn)LIB_SYM(
        g_core_lib, "execute_command_with_callback");
    if (!g_execute_command_with_callback) {
        napi_throw_error(env, NULL, "Failed to resolve 'execute_command_with_callback' symbol");
        return NULL;
    }

    g_execute_command_with_binary = (ExecuteCommandWithBinaryFn)LIB_SYM(
        g_core_lib, "execute_command_with_binary");
    if (!g_execute_command_with_binary) {
        napi_throw_error(env, NULL, "Failed to resolve 'execute_command_with_binary' symbol");
        return NULL;
    }

    napi_value undefined;
    NAPI_CALL(env, napi_get_undefined(env, &undefined));
    return undefined;
}

/* ── executeCommand(command, dataJson) ────────────────────────────────── */

static napi_value napi_execute_command(napi_env env, napi_callback_info info) {
    if (!g_execute_command) {
        napi_throw_error(env, NULL, "Native library not loaded. Call loadLibrary() first.");
        return NULL;
    }

    size_t argc = 2;
    napi_value argv[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    if (argc < 2) {
        napi_throw_error(env, NULL, "executeCommand requires 2 arguments (command, dataJson)");
        return NULL;
    }

    /* Extract command string */
    size_t cmd_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], NULL, 0, &cmd_len));
    char* cmd = (char*)malloc(cmd_len + 1);
    if (!cmd) { napi_throw_error(env, NULL, "Out of memory"); return NULL; }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], cmd, cmd_len + 1, &cmd_len));

    /* Extract data JSON string */
    size_t data_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], NULL, 0, &data_len));
    char* data = (char*)malloc(data_len + 1);
    if (!data) { free(cmd); napi_throw_error(env, NULL, "Out of memory"); return NULL; }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], data, data_len + 1, &data_len));

    RequestBuffer req = {
        .Command = cmd,
        .CommandLength = (int32_t)cmd_len,
        .Data = data,
        .DataLength = (int32_t)data_len
    };
    ResponseBuffer res = { NULL, 0, NULL, 0 };

    g_execute_command(&req, &res);

    napi_value result = handle_response(env, cmd, &res);

    free(cmd);
    free(data);
    return result;
}

/* ── executeCommandWithBinary(command, dataJson, binaryBuffer) ────────── */

static napi_value napi_execute_command_with_binary(napi_env env,
                                                    napi_callback_info info) {
    if (!g_execute_command_with_binary) {
        napi_throw_error(env, NULL, "Native library not loaded. Call loadLibrary() first.");
        return NULL;
    }

    size_t argc = 3;
    napi_value argv[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    if (argc < 3) {
        napi_throw_error(env, NULL,
            "executeCommandWithBinary requires 3 arguments (command, dataJson, binaryBuffer)");
        return NULL;
    }

    /* Extract command string */
    size_t cmd_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], NULL, 0, &cmd_len));
    char* cmd = (char*)malloc(cmd_len + 1);
    if (!cmd) { napi_throw_error(env, NULL, "Out of memory"); return NULL; }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], cmd, cmd_len + 1, &cmd_len));

    /* Extract data JSON string */
    size_t data_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], NULL, 0, &data_len));
    char* data = (char*)malloc(data_len + 1);
    if (!data) { free(cmd); napi_throw_error(env, NULL, "Out of memory"); return NULL; }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], data, data_len + 1, &data_len));

    /* Extract binary buffer */
    void* bin_data = NULL;
    size_t bin_len = 0;
    bool is_buffer = false;
    NAPI_CALL(env, napi_is_buffer(env, argv[2], &is_buffer));

    if (is_buffer) {
        NAPI_CALL(env, napi_get_buffer_info(env, argv[2], &bin_data, &bin_len));
    } else {
        bool is_typedarray = false;
        NAPI_CALL(env, napi_is_typedarray(env, argv[2], &is_typedarray));
        if (is_typedarray) {
            napi_typedarray_type type;
            size_t length;
            void* arr_data;
            napi_value arr_buf;
            size_t offset;
            NAPI_CALL(env, napi_get_typedarray_info(env, argv[2], &type, &length,
                                                     &arr_data, &arr_buf, &offset));
            bin_data = arr_data;
            bin_len = length;
        } else {
            free(cmd);
            free(data);
            napi_throw_type_error(env, NULL,
                "binaryBuffer must be a Buffer or Uint8Array");
            return NULL;
        }
    }

    StreamingRequestBuffer req = {
        .Command = cmd,
        .CommandLength = (int32_t)cmd_len,
        .Data = data,
        .DataLength = (int32_t)data_len,
        .BinaryData = bin_data,
        .BinaryDataLength = (int32_t)bin_len
    };
    ResponseBuffer res = { NULL, 0, NULL, 0 };

    g_execute_command_with_binary(&req, &res);

    napi_value result = handle_response(env, cmd, &res);

    free(cmd);
    free(data);
    return result;
}

/* ── Streaming async work data ────────────────────────────────────────── */

typedef struct {
    /* Input (owned, freed after work completes) */
    char* command;
    size_t command_length;
    char* data;
    size_t data_length;

    /* Threadsafe function for streaming callback */
    napi_threadsafe_function tsfn;

    /* Output from native call */
    ResponseBuffer response;

    /* Promise */
    napi_deferred deferred;
    napi_async_work work;
} StreamingWorkData;

/* Called on the JS thread when the native callback fires */
static void streaming_call_js(napi_env env, napi_value js_callback,
                               void* context, void* data) {
    if (!env || !data) return;

    /* data is a heap-allocated copy of the chunk */
    char* chunk = (char*)data;
    size_t chunk_len = strlen(chunk);

    napi_value argv[1];
    napi_value global;
    napi_status status;

    status = napi_create_string_utf8(env, chunk, chunk_len, &argv[0]);
    free(chunk);

    if (status != napi_ok) return;

    status = napi_get_global(env, &global);
    if (status != napi_ok) return;

    napi_value result;
    /* Ignore return value – callback is fire-and-forget */
    napi_call_function(env, global, js_callback, 1, argv, &result);
}

/* Native callback trampoline invoked by the core library (possibly from
   a worker thread). Copies chunk data and dispatches to the JS thread
   via threadsafe function. Returns 0 to continue, 1 to cancel. */
static int32_t streaming_native_callback(const void* data, int32_t length,
                                          void* userData) {
    StreamingWorkData* work_data = (StreamingWorkData*)userData;
    if (!work_data || !work_data->tsfn || !data || length <= 0) {
        return 0; /* continue even on unexpected state */
    }

    /* Heap-copy the chunk so it survives until the JS thread picks it up */
    char* chunk_copy = (char*)malloc((size_t)length + 1);
    if (!chunk_copy) return 1; /* cancel on OOM */
    memcpy(chunk_copy, data, (size_t)length);
    chunk_copy[length] = '\0';

    napi_status status = napi_call_threadsafe_function(
        work_data->tsfn, chunk_copy, napi_tsfn_blocking);
    if (status != napi_ok) {
        free(chunk_copy);
        return 1; /* cancel */
    }

    return 0; /* continue */
}

/* Runs on the libuv worker thread – must NOT call napi_* (except tsfn) */
static void streaming_execute(napi_env env, void* data) {
    StreamingWorkData* work_data = (StreamingWorkData*)data;

    RequestBuffer req = {
        .Command = work_data->command,
        .CommandLength = (int32_t)work_data->command_length,
        .Data = work_data->data,
        .DataLength = (int32_t)work_data->data_length
    };

    work_data->response.Data = NULL;
    work_data->response.DataLength = 0;
    work_data->response.Error = NULL;
    work_data->response.ErrorLength = 0;

    g_execute_command_with_callback(
        &req, &work_data->response,
        streaming_native_callback, work_data);
}

/* Runs on the JS main thread after streaming_execute completes */
static void streaming_complete(napi_env env, napi_status status, void* data) {
    StreamingWorkData* work_data = (StreamingWorkData*)data;

    /* Release the threadsafe function */
    napi_release_threadsafe_function(work_data->tsfn, napi_tsfn_release);

    if (status == napi_cancelled) {
        napi_value err_val;
        napi_create_string_utf8(env, "Async work cancelled", NAPI_AUTO_LENGTH,
                                &err_val);
        napi_reject_deferred(env, work_data->deferred, err_val);
    } else if (work_data->response.Error && work_data->response.ErrorLength > 0) {
        /* Build error message */
        int32_t elen = work_data->response.ErrorLength;
        size_t msg_size = (size_t)elen + 128;
        char* msg = (char*)malloc(msg_size);
        if (msg) {
            snprintf(msg, msg_size, "Command '%s' failed: %.*s",
                     work_data->command, elen,
                     (const char*)work_data->response.Error);
            napi_value err_val;
            napi_create_string_utf8(env, msg, NAPI_AUTO_LENGTH, &err_val);
            napi_reject_deferred(env, work_data->deferred, err_val);
            free(msg);
        } else {
            napi_value err_val;
            napi_create_string_utf8(env, "Command failed (OOM)", NAPI_AUTO_LENGTH,
                                    &err_val);
            napi_reject_deferred(env, work_data->deferred, err_val);
        }
    } else {
        napi_value result;
        if (work_data->response.Data && work_data->response.DataLength > 0) {
            napi_create_string_utf8(env,
                (const char*)work_data->response.Data,
                work_data->response.DataLength, &result);
        } else {
            napi_create_string_utf8(env, "", 0, &result);
        }
        napi_resolve_deferred(env, work_data->deferred, result);
    }

    /* Free native response buffers */
    free_native_buffer(work_data->response.Data);
    free_native_buffer(work_data->response.Error);

    /* Free work data */
    napi_delete_async_work(env, work_data->work);
    free(work_data->command);
    free(work_data->data);
    free(work_data);
}

/* ── executeCommandStreaming(command, dataJson, callback) → Promise ───── */

static napi_value napi_execute_command_streaming(napi_env env,
                                                  napi_callback_info info) {
    if (!g_execute_command_with_callback) {
        napi_throw_error(env, NULL, "Native library not loaded. Call loadLibrary() first.");
        return NULL;
    }

    size_t argc = 3;
    napi_value argv[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    if (argc < 3) {
        napi_throw_error(env, NULL,
            "executeCommandStreaming requires 3 arguments (command, dataJson, callback)");
        return NULL;
    }

    /* Verify callback is a function */
    napi_valuetype cb_type;
    NAPI_CALL(env, napi_typeof(env, argv[2], &cb_type));
    if (cb_type != napi_function) {
        napi_throw_type_error(env, NULL, "Third argument must be a function");
        return NULL;
    }

    /* Extract command string */
    size_t cmd_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], NULL, 0, &cmd_len));
    char* cmd = (char*)malloc(cmd_len + 1);
    if (!cmd) { napi_throw_error(env, NULL, "Out of memory"); return NULL; }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[0], cmd, cmd_len + 1, &cmd_len));

    /* Extract data JSON string */
    size_t data_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], NULL, 0, &data_len));
    char* data_str = (char*)malloc(data_len + 1);
    if (!data_str) {
        free(cmd);
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    NAPI_CALL(env, napi_get_value_string_utf8(env, argv[1], data_str, data_len + 1, &data_len));

    /* Allocate work data */
    StreamingWorkData* work_data = (StreamingWorkData*)calloc(1, sizeof(StreamingWorkData));
    if (!work_data) {
        free(cmd);
        free(data_str);
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    work_data->command = cmd;
    work_data->command_length = cmd_len;
    work_data->data = data_str;
    work_data->data_length = data_len;

    /* Create Promise */
    napi_value promise;
    NAPI_CALL(env, napi_create_promise(env, &work_data->deferred, &promise));

    /* Create threadsafe function for streaming callback */
    napi_value resource_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "foundry_streaming_cb",
                                            NAPI_AUTO_LENGTH, &resource_name));
    NAPI_CALL(env, napi_create_threadsafe_function(
        env, argv[2], NULL, resource_name,
        0,    /* max_queue_size: 0 = unlimited */
        1,    /* initial_thread_count */
        NULL, /* thread_finalize_data */
        NULL, /* thread_finalize_cb */
        NULL, /* context */
        streaming_call_js,
        &work_data->tsfn));

    /* Create async work */
    napi_value work_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "foundry_streaming_work",
                                            NAPI_AUTO_LENGTH, &work_name));
    NAPI_CALL(env, napi_create_async_work(env, NULL, work_name,
                                           streaming_execute,
                                           streaming_complete,
                                           work_data,
                                           &work_data->work));

    /* Queue the work */
    napi_status queue_status = napi_queue_async_work(env, work_data->work);
    if (queue_status != napi_ok) {
        napi_release_threadsafe_function(work_data->tsfn, napi_tsfn_release);
        napi_delete_async_work(env, work_data->work);
        free(work_data->command);
        free(work_data->data);
        free(work_data);
        napi_throw_error(env, NULL, "Failed to queue async work");
        return NULL;
    }

    return promise;
}

/* ── Module initialization ────────────────────────────────────────────── */

static napi_value init(napi_env env, napi_value exports) {
    napi_property_descriptor props[] = {
        { "loadLibrary", NULL, napi_load_library, NULL, NULL, NULL,
          napi_default, NULL },
        { "executeCommand", NULL, napi_execute_command, NULL, NULL, NULL,
          napi_default, NULL },
        { "executeCommandWithBinary", NULL, napi_execute_command_with_binary,
          NULL, NULL, NULL, napi_default, NULL },
        { "executeCommandStreaming", NULL, napi_execute_command_streaming,
          NULL, NULL, NULL, napi_default, NULL },
    };

    NAPI_CALL(env, napi_define_properties(env, exports,
        sizeof(props) / sizeof(props[0]), props));

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
