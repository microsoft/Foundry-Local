//! FFI bridge to the `Microsoft.AI.Foundry.Local.Core` native library.
//!
//! Dynamically loads the shared library at runtime via [`libloading`] and
//! exposes two operations:
//!
//! * [`CoreInterop::execute_command`] – synchronous request/response.
//! * [`CoreInterop::execute_command_streaming`] – request with a streaming
//!   callback that receives incremental chunks.

use std::ffi::CString;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use libloading::{Library, Symbol};
use serde_json::Value;

use crate::configuration::Configuration;
use crate::error::{FoundryLocalError, Result};

// ── FFI types ────────────────────────────────────────────────────────────────

/// Request buffer passed to the native library.
#[repr(C)]
struct RequestBuffer {
    command: *const i8,
    command_length: i32,
    data: *const i8,
    data_length: i32,
}

/// Response buffer filled by the native library.
#[repr(C)]
struct ResponseBuffer {
    data: *mut u8,
    data_length: u32,
    error: *mut u8,
    error_length: u32,
}

impl ResponseBuffer {
    fn new() -> Self {
        Self {
            data: std::ptr::null_mut(),
            data_length: 0,
            error: std::ptr::null_mut(),
            error_length: 0,
        }
    }
}

/// Signature for `execute_command`.
type ExecuteCommandFn = unsafe extern "C" fn(*const RequestBuffer, *mut ResponseBuffer);

/// Signature for the streaming callback invoked by the native library.
/// Returns 0 to continue, 1 to cancel.
type CallbackFn = unsafe extern "C" fn(*const u8, i32, *mut std::ffi::c_void) -> i32;

/// Signature for `execute_command_with_callback`.
type ExecuteCommandWithCallbackFn = unsafe extern "C" fn(
    *const RequestBuffer,
    *mut ResponseBuffer,
    CallbackFn,
    *mut std::ffi::c_void,
);

// ── Library name helpers ─────────────────────────────────────────────────────

#[cfg(target_os = "windows")]
const LIB_EXTENSION: &str = "dll";
#[cfg(target_os = "macos")]
const LIB_EXTENSION: &str = "dylib";
#[cfg(target_os = "linux")]
const LIB_EXTENSION: &str = "so";

// ── Native buffer deallocation ────────────────────────────────────────────────

/// Free a buffer allocated by the native core library.
///
/// The .NET native core allocates response buffers with
/// `Marshal.AllocHGlobal` which maps to `malloc` on Unix and
/// `LocalAlloc` (process heap) on Windows.  The corresponding
/// free functions are `free` and `LocalFree` respectively.
///
/// # Safety
///
/// `ptr` must be null or a valid pointer previously allocated by the native
/// core library via the corresponding platform allocator. Calling this with
/// any other pointer is undefined behaviour.
unsafe fn free_native_buffer(ptr: *mut u8) {
    if ptr.is_null() {
        return;
    }
    #[cfg(unix)]
    {
        extern "C" {
            fn free(ptr: *mut std::ffi::c_void);
        }
        // SAFETY: `ptr` was allocated by the native core via `malloc` on Unix.
        free(ptr as *mut std::ffi::c_void);
    }
    #[cfg(windows)]
    {
        extern "system" {
            fn LocalFree(hMem: *mut std::ffi::c_void) -> *mut std::ffi::c_void;
        }
        // SAFETY: `ptr` was allocated by the native core via `LocalAlloc`
        // (Marshal.AllocHGlobal) on Windows.
        LocalFree(ptr as *mut std::ffi::c_void);
    }
}

// ── Trampoline for streaming callback ────────────────────────────────────────

/// C-ABI trampoline that forwards chunks from the native library into a Rust
/// closure stored behind `user_data`.
///
/// Wrapped in [`std::panic::catch_unwind`] so that a panic inside the Rust
/// closure cannot unwind across the FFI boundary (which is undefined behaviour).
///
/// # Safety
/// State carried through the FFI callback for streaming, including a buffer to
/// accumulate partial UTF-8 sequences that may be split across native callbacks.
///
/// The callback is stored as a raw mutable pointer to avoid `'static` bounds —
/// the caller guarantees the referenced closure outlives this struct.
struct StreamingCallbackState<'a> {
    callback: &'a mut dyn FnMut(&str),
    buf: Vec<u8>,
    cancel_flag: Option<Arc<AtomicBool>>,
}

impl<'a> StreamingCallbackState<'a> {
    fn new(callback: &'a mut dyn FnMut(&str)) -> Self {
        Self {
            callback,
            buf: Vec::new(),
            cancel_flag: None,
        }
    }

    fn with_cancel(callback: &'a mut dyn FnMut(&str), cancel_flag: Arc<AtomicBool>) -> Self {
        Self {
            callback,
            buf: Vec::new(),
            cancel_flag: Some(cancel_flag),
        }
    }

    /// Returns `true` if cancellation has been requested.
    fn is_cancelled(&self) -> bool {
        self.cancel_flag
            .as_ref()
            .is_some_and(|f| f.load(Ordering::Relaxed))
    }

    /// Append raw bytes, decode as much valid UTF-8 as possible, and forward
    /// complete text to the callback. Any trailing incomplete multi-byte
    /// sequence is kept in the buffer for the next call. Invalid byte
    /// sequences are skipped to prevent the buffer from growing unboundedly.
    fn push(&mut self, bytes: &[u8]) {
        self.buf.extend_from_slice(bytes);
        loop {
            match std::str::from_utf8(&self.buf) {
                Ok(s) => {
                    if !s.is_empty() {
                        (self.callback)(s);
                    }
                    self.buf.clear();
                    break;
                }
                Err(e) => {
                    let n = e.valid_up_to();
                    if n > 0 {
                        // SAFETY: `valid_up_to` guarantees this prefix is valid UTF-8.
                        let valid = unsafe { std::str::from_utf8_unchecked(&self.buf[..n]) };
                        (self.callback)(valid);
                    }
                    match e.error_len() {
                        Some(err_len) => {
                            // Definite invalid sequence — skip past it and
                            // continue decoding the remainder.
                            self.buf.drain(..n + err_len);
                        }
                        None => {
                            // Incomplete multi-byte sequence at the end —
                            // keep it for the next push.
                            self.buf.drain(..n);
                            break;
                        }
                    }
                }
            }
        }
    }

    /// Flush any remaining bytes as lossy UTF-8 (called once after the native
    /// call completes).
    fn flush(&mut self) {
        if !self.buf.is_empty() {
            let text = String::from_utf8_lossy(&self.buf).into_owned();
            (self.callback)(&text);
            self.buf.clear();
        }
    }
}

///
/// * `data` must be a valid pointer to `length` bytes of UTF-8 (or at least
///   valid memory) allocated by the native core, valid for the duration of
///   this call.
/// * `user_data` must point to a live [`StreamingCallbackState`] that was
///   created by [`CoreInterop::execute_command_streaming`] and has not been
///   dropped.
unsafe extern "C" fn streaming_trampoline(
    data: *const u8,
    length: i32,
    user_data: *mut std::ffi::c_void,
) -> i32 {
    if data.is_null() || length <= 0 {
        return 0;
    }
    // catch_unwind prevents UB if the closure panics across the FFI boundary.
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        // SAFETY: `user_data` points to a `StreamingCallbackState` kept alive
        // by the caller of `execute_command_with_callback` for the duration of
        // the native call.
        let state = &mut *(user_data as *mut StreamingCallbackState<'_>);

        // Check for cancellation before processing the chunk.
        if state.is_cancelled() {
            return 1; // cancel
        }

        // SAFETY: `data` is valid for `length` bytes as guaranteed by the native
        // core's callback contract.
        let slice = std::slice::from_raw_parts(data, length as usize);
        state.push(slice);
        0 // continue
    }));
    match result {
        Ok(ret) => ret,
        Err(_) => 1,
    }
}

// ── CoreInterop ──────────────────────────────────────────────────────────────

/// Handle to the loaded native core library.
///
/// This type is `Send + Sync` because the underlying native library is
/// expected to be thread-safe for distinct request/response pairs.
pub(crate) struct CoreInterop {
    _library: Library,
    #[cfg(target_os = "windows")]
    _dependency_libs: Vec<Library>,
    execute_command: unsafe extern "C" fn(*const RequestBuffer, *mut ResponseBuffer),
    execute_command_with_callback: unsafe extern "C" fn(
        *const RequestBuffer,
        *mut ResponseBuffer,
        CallbackFn,
        *mut std::ffi::c_void,
    ),
}

impl std::fmt::Debug for CoreInterop {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CoreInterop").finish_non_exhaustive()
    }
}

impl CoreInterop {
    /// Load the native core library using the provided configuration to locate
    /// it on disk.
    ///
    /// Discovery order:
    /// 1. `FoundryLocalCorePath` key in `config.params`.
    /// 2. Sibling directory of the current executable.
    pub fn new(config: &mut Configuration) -> Result<Self> {
        let lib_path = Self::resolve_library_path(config)?;

        // Auto-detect WinAppSDK Bootstrap DLL next to the core library.
        // If present, tell the native core to run the bootstrapper during
        // initialisation — this is required for WinML execution providers.
        #[cfg(target_os = "windows")]
        if !config.params.contains_key("Bootstrap") {
            if let Some(dir) = lib_path.parent() {
                if dir
                    .join("Microsoft.WindowsAppRuntime.Bootstrap.dll")
                    .exists()
                {
                    config.params.insert("Bootstrap".into(), "true".into());
                }
            }
        }

        #[cfg(target_os = "windows")]
        let _dependency_libs = Self::load_windows_dependencies(&lib_path)?;

        // SAFETY: `lib_path` has been verified to exist on disk. Loading a
        // shared library is inherently unsafe (it executes foreign code), but
        // the path is resolved from trusted configuration sources.
        let library = unsafe {
            Library::new(&lib_path).map_err(|e| FoundryLocalError::LibraryLoad {
                reason: format!(
                    "Failed to load native library at {}: {e}",
                    lib_path.display()
                ),
            })?
        };

        // SAFETY: We trust the loaded library to export these symbols with the
        // correct C-ABI signatures as defined by the Foundry Local native core.
        let execute_command: ExecuteCommandFn = unsafe {
            let sym: Symbol<ExecuteCommandFn> =
                library
                    .get(b"execute_command\0")
                    .map_err(|e| FoundryLocalError::LibraryLoad {
                        reason: format!("Symbol 'execute_command' not found: {e}"),
                    })?;
            *sym
        };

        // SAFETY: Same as above — symbol must match `ExecuteCommandWithCallbackFn`.
        let execute_command_with_callback: ExecuteCommandWithCallbackFn = unsafe {
            let sym: Symbol<ExecuteCommandWithCallbackFn> = library
                .get(b"execute_command_with_callback\0")
                .map_err(|e| FoundryLocalError::LibraryLoad {
                    reason: format!("Symbol 'execute_command_with_callback' not found: {e}"),
                })?;
            *sym
        };

        Ok(Self {
            _library: library,
            #[cfg(target_os = "windows")]
            _dependency_libs,
            execute_command,
            execute_command_with_callback,
        })
    }

    /// Execute a synchronous command against the native core.
    ///
    /// `command` is the operation name (e.g. `"initialize"`, `"load_model"`).
    /// `params` is an optional JSON value that will be serialised and sent as
    /// the data payload.
    pub fn execute_command(&self, command: &str, params: Option<&Value>) -> Result<String> {
        let cmd = CString::new(command).map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("Invalid command string: {e}"),
        })?;

        let data_json = match params {
            Some(v) => serde_json::to_string(v)?,
            None => String::new(),
        };
        let data_cstr =
            CString::new(data_json.as_str()).map_err(|e| FoundryLocalError::CommandExecution {
                reason: format!("Invalid data string: {e}"),
            })?;

        let request = RequestBuffer {
            command: cmd.as_ptr(),
            command_length: cmd.as_bytes().len() as i32,
            data: data_cstr.as_ptr(),
            data_length: data_cstr.as_bytes().len() as i32,
        };

        let mut response = ResponseBuffer::new();

        // SAFETY: `request` fields point into `cmd` and `data_cstr` which are
        // alive for the duration of this call. The native function writes into
        // `response` using its documented C ABI.
        unsafe {
            (self.execute_command)(&request, &mut response);
        }

        Self::process_response(response)
    }

    /// Execute a command that streams results back via `callback`.
    ///
    /// Each chunk delivered by the native library is decoded as UTF-8 and
    /// forwarded to `callback`. After the native call returns, any error in
    /// the response buffer is raised.
    pub fn execute_command_streaming<F>(
        &self,
        command: &str,
        params: Option<&Value>,
        mut callback: F,
    ) -> Result<String>
    where
        F: FnMut(&str),
    {
        self.execute_command_streaming_impl(command, params, &mut callback, None)
    }

    /// Like [`Self::execute_command_streaming`], but accepts a cancellation
    /// flag. When `cancel_flag` is set to `true`, the native call will be
    /// cancelled at the next callback invocation and an error is returned.
    pub fn execute_command_streaming_cancellable<F>(
        &self,
        command: &str,
        params: Option<&Value>,
        mut callback: F,
        cancel_flag: Arc<AtomicBool>,
    ) -> Result<String>
    where
        F: FnMut(&str),
    {
        self.execute_command_streaming_impl(command, params, &mut callback, Some(cancel_flag))
    }

    fn execute_command_streaming_impl(
        &self,
        command: &str,
        params: Option<&Value>,
        callback: &mut dyn FnMut(&str),
        cancel_flag: Option<Arc<AtomicBool>>,
    ) -> Result<String> {
        let cmd = CString::new(command).map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("Invalid command string: {e}"),
        })?;

        let data_json = match params {
            Some(v) => serde_json::to_string(v)?,
            None => String::new(),
        };
        let data_cstr =
            CString::new(data_json.as_str()).map_err(|e| FoundryLocalError::CommandExecution {
                reason: format!("Invalid data string: {e}"),
            })?;

        let request = RequestBuffer {
            command: cmd.as_ptr(),
            command_length: cmd.as_bytes().len() as i32,
            data: data_cstr.as_ptr(),
            data_length: data_cstr.as_bytes().len() as i32,
        };

        let mut response = ResponseBuffer::new();

        // Wrap the closure in a StreamingCallbackState that handles partial
        // UTF-8 sequences split across native callbacks.
        let mut state = match cancel_flag {
            Some(flag) => StreamingCallbackState::with_cancel(callback, flag),
            None => StreamingCallbackState::new(callback),
        };
        let user_data = &mut state as *mut StreamingCallbackState<'_> as *mut std::ffi::c_void;

        // SAFETY: `request` fields point into `cmd` and `data_cstr` which are
        // alive for the duration of this call. `user_data` points to `state`
        // which lives on this stack frame and outlives the native call.
        // `streaming_trampoline` will only cast `user_data` back to
        // `StreamingCallbackState`.
        unsafe {
            (self.execute_command_with_callback)(
                &request,
                &mut response,
                streaming_trampoline,
                user_data,
            );
        }

        let cancelled = state.is_cancelled();

        // Flush any trailing partial UTF-8 bytes.
        state.flush();

        if cancelled {
            // Free native response memory before returning the error.
            Self::process_response(response).ok();
            return Err(FoundryLocalError::CommandExecution {
                reason: "Operation cancelled".to_string(),
            });
        }

        Self::process_response(response)
    }

    /// Async version of [`Self::execute_command`].
    ///
    /// Runs the blocking FFI call on a dedicated thread via
    /// [`tokio::task::spawn_blocking`] so the async runtime is never blocked.
    pub async fn execute_command_async(
        self: &Arc<Self>,
        command: String,
        params: Option<Value>,
    ) -> Result<String> {
        let this = Arc::clone(self);
        tokio::task::spawn_blocking(move || this.execute_command(&command, params.as_ref()))
            .await
            .map_err(|e| FoundryLocalError::CommandExecution {
                reason: format!("task join error: {e}"),
            })?
    }

    /// Async version of [`Self::execute_command_streaming`].
    ///
    /// The `callback` is invoked on the blocking thread – it must be
    /// [`Send`] + `'static`.
    pub async fn execute_command_streaming_async<F>(
        self: &Arc<Self>,
        command: String,
        params: Option<Value>,
        callback: F,
    ) -> Result<String>
    where
        F: FnMut(&str) + Send + 'static,
    {
        let this = Arc::clone(self);
        tokio::task::spawn_blocking(move || {
            this.execute_command_streaming(&command, params.as_ref(), callback)
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("task join error: {e}"),
        })?
    }

    /// Async version of [`Self::execute_command_streaming_cancellable`].
    ///
    /// Accepts a shared cancellation flag (`Arc<AtomicBool>`). When the flag
    /// is set to `true`, the native call will be cancelled at the next
    /// callback invocation and an error is returned.
    pub async fn execute_command_streaming_cancellable_async<F>(
        self: &Arc<Self>,
        command: String,
        params: Option<Value>,
        callback: F,
        cancel_flag: Arc<AtomicBool>,
    ) -> Result<String>
    where
        F: FnMut(&str) + Send + 'static,
    {
        let this = Arc::clone(self);
        tokio::task::spawn_blocking(move || {
            this.execute_command_streaming_cancellable(
                &command,
                params.as_ref(),
                callback,
                cancel_flag,
            )
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("task join error: {e}"),
        })?
    }

    /// Async streaming variant that bridges the FFI callback into a
    /// [`tokio::sync::mpsc`] channel.
    ///
    /// Returns a `Receiver<Result<String>>` that yields each chunk as it
    /// arrives.  After all chunks have been delivered the final result from
    /// the native core response buffer is sent through the same channel —
    /// if the native core reported an error it will appear as an `Err` item.
    /// The receiver can be wrapped with
    /// [`tokio_stream::wrappers::ReceiverStream`] to get a `Stream`.
    pub async fn execute_command_streaming_channel(
        self: &Arc<Self>,
        command: String,
        params: Option<Value>,
    ) -> Result<tokio::sync::mpsc::UnboundedReceiver<Result<String>>> {
        let (tx, rx) = tokio::sync::mpsc::unbounded_channel::<Result<String>>();
        let this = Arc::clone(self);

        tokio::task::spawn_blocking(move || {
            let tx_chunk = tx.clone();
            let result =
                this.execute_command_streaming(&command, params.as_ref(), move |chunk: &str| {
                    let _ = tx_chunk.send(Ok(chunk.to_owned()));
                });

            match result {
                Ok(_final_payload) => {
                    // The native core's response buffer typically contains a
                    // status/summary string, not a stream chunk. Dropping it is
                    // intentional — all meaningful data was already sent via
                    // the streaming callback.
                }
                Err(e) => {
                    let _ = tx.send(Err(e));
                }
            }
        });

        Ok(rx)
    }

    /// Read a native response buffer field as a Rust `String`.
    ///
    /// # Safety
    ///
    /// `ptr` must be null **or** a valid pointer to at least `len` bytes of
    /// memory allocated by the native core. The memory must remain valid for
    /// the duration of this call.
    unsafe fn read_native_buffer(ptr: *mut u8, len: u32) -> Option<String> {
        if ptr.is_null() || len == 0 {
            return None;
        }
        // SAFETY: caller guarantees `ptr` is valid for `len` bytes.
        let slice = std::slice::from_raw_parts(ptr, len as usize);
        Some(String::from_utf8_lossy(slice).into_owned())
    }

    /// Read the response buffer, free the native memory, and return the data
    /// string or raise an error.
    ///
    /// Takes the buffer by value so it can only be consumed once.
    fn process_response(response: ResponseBuffer) -> Result<String> {
        // SAFETY: response fields are either null or valid native-allocated
        // pointers filled by the preceding FFI call.
        let error_str = unsafe { Self::read_native_buffer(response.error, response.error_length) };
        let data_str = unsafe { Self::read_native_buffer(response.data, response.data_length) };

        // SAFETY: Free the heap-allocated response buffers (matches JS
        // koffi.free() and C# Marshal.FreeHGlobal() behaviour). Each pointer
        // is either null (handled inside free_native_buffer) or was allocated
        // by the native core's platform allocator.
        unsafe {
            free_native_buffer(response.data);
            free_native_buffer(response.error);
        }

        // Return error or data.
        if let Some(err) = error_str {
            Err(FoundryLocalError::CommandExecution { reason: err })
        } else {
            Ok(data_str.unwrap_or_default())
        }
    }

    /// Resolve the path to the native core shared library.
    fn resolve_library_path(config: &Configuration) -> Result<PathBuf> {
        let lib_name = format!("Microsoft.AI.Foundry.Local.Core.{LIB_EXTENSION}");

        // 1. Explicit path from configuration.
        if let Some(dir) = config.params.get("FoundryLocalCorePath") {
            let p = Path::new(dir).join(&lib_name);
            if p.exists() {
                return Ok(p);
            }
            // If the config value is the full path to the library itself.
            let p = Path::new(dir);
            if p.exists() && p.is_file() {
                return Ok(p.to_path_buf());
            }
        }

        // 2. Compile-time path set by build.rs (points at the OUT_DIR where
        //    native NuGet packages are extracted during `cargo build`).
        if let Some(dir) = option_env!("FOUNDRY_NATIVE_DIR") {
            let p = Path::new(dir).join(&lib_name);
            if p.exists() {
                return Ok(p);
            }
        }

        // 3. Next to the running executable (default search path).
        if let Ok(exe) = std::env::current_exe() {
            if let Some(dir) = exe.parent() {
                let p = dir.join(&lib_name);
                if p.exists() {
                    return Ok(p);
                }
            }
        }

        Err(FoundryLocalError::LibraryLoad {
            reason: format!(
                "Could not locate native library '{lib_name}'. \
                 Set the FoundryLocalCorePath config option."
            ),
        })
    }

    /// On Windows, pre-load runtime dependencies so the core library can
    /// resolve them.
    #[cfg(target_os = "windows")]
    fn load_windows_dependencies(core_lib_path: &Path) -> Result<Vec<Library>> {
        let dir = core_lib_path.parent().unwrap_or_else(|| Path::new("."));

        let mut libs = Vec::new();

        // Load WinML bootstrap if present.
        let bootstrap = dir.join("Microsoft.WindowsAppRuntime.Bootstrap.dll");
        if bootstrap.exists() {
            // SAFETY: Pre-loading a known dependency DLL from the same trusted
            // directory as the core library.
            if let Ok(lib) = unsafe { Library::new(&bootstrap) } {
                libs.push(lib);
            }
        }

        for dep in &["onnxruntime.dll", "onnxruntime-genai.dll"] {
            let dep_path = dir.join(dep);
            if dep_path.exists() {
                // SAFETY: Pre-loading a known dependency DLL from the same
                // trusted directory as the core library.
                let lib = unsafe {
                    Library::new(&dep_path).map_err(|e| FoundryLocalError::LibraryLoad {
                        reason: format!("Failed to load dependency {dep}: {e}"),
                    })?
                };
                libs.push(lib);
            }
        }

        Ok(libs)
    }
}
