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
    data_length: i32,
    error: *mut u8,
    error_length: i32,
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
type CallbackFn = unsafe extern "C" fn(*const u8, i32, *mut std::ffi::c_void);

/// Signature for `execute_command_with_callback`.
type ExecuteCommandWithCallbackFn =
    unsafe extern "C" fn(*const RequestBuffer, *mut ResponseBuffer, CallbackFn, *mut std::ffi::c_void);

// ── Library name helpers ─────────────────────────────────────────────────────

#[cfg(target_os = "windows")]
const CORE_LIB_NAME: &str = "Microsoft.AI.Foundry.Local.Core.dll";
#[cfg(target_os = "macos")]
const CORE_LIB_NAME: &str = "Microsoft.AI.Foundry.Local.Core.dylib";
#[cfg(target_os = "linux")]
const CORE_LIB_NAME: &str = "Microsoft.AI.Foundry.Local.Core.so";

// ── Native buffer deallocation ────────────────────────────────────────────────

/// Free a buffer allocated by the native core library.
///
/// The .NET native core allocates response buffers with
/// `Marshal.AllocHGlobal` which maps to `malloc` on Unix and
/// `CoTaskMemAlloc` on Windows.
unsafe fn free_native_buffer(ptr: *mut u8) {
    if ptr.is_null() {
        return;
    }
    #[cfg(unix)]
    {
        extern "C" {
            fn free(ptr: *mut std::ffi::c_void);
        }
        free(ptr as *mut std::ffi::c_void);
    }
    #[cfg(windows)]
    {
        extern "system" {
            fn CoTaskMemFree(pv: *mut std::ffi::c_void);
        }
        CoTaskMemFree(ptr as *mut std::ffi::c_void);
    }
}

// ── Trampoline for streaming callback ────────────────────────────────────────

/// C-ABI trampoline that forwards chunks from the native library into a Rust
/// closure stored behind `user_data`.
unsafe extern "C" fn streaming_trampoline(
    data: *const u8,
    length: i32,
    user_data: *mut std::ffi::c_void,
) {
    if data.is_null() || length <= 0 {
        return;
    }
    let closure = &mut *(user_data as *mut Box<dyn FnMut(&str)>);
    let slice = std::slice::from_raw_parts(data, length as usize);
    if let Ok(chunk) = std::str::from_utf8(slice) {
        closure(chunk);
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
    execute_command_with_callback:
        unsafe extern "C" fn(*const RequestBuffer, *mut ResponseBuffer, CallbackFn, *mut std::ffi::c_void),
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
    /// 2. `FOUNDRY_NATIVE_DIR` environment variable.
    /// 3. Sibling directory of the current executable.
    pub fn new(config: &Configuration) -> Result<Self> {
        let lib_path = Self::resolve_library_path(config)?;

        #[cfg(target_os = "windows")]
        let _dependency_libs = Self::load_windows_dependencies(&lib_path)?;

        let library = unsafe {
            Library::new(&lib_path).map_err(|e| {
                FoundryLocalError::LibraryLoad(format!(
                    "Failed to load native library at {}: {e}",
                    lib_path.display()
                ))
            })?
        };

        let execute_command: ExecuteCommandFn = unsafe {
            let sym: Symbol<ExecuteCommandFn> =
                library.get(b"execute_command\0").map_err(|e| {
                    FoundryLocalError::LibraryLoad(format!(
                        "Symbol 'execute_command' not found: {e}"
                    ))
                })?;
            *sym
        };

        let execute_command_with_callback: ExecuteCommandWithCallbackFn = unsafe {
            let sym: Symbol<ExecuteCommandWithCallbackFn> =
                library.get(b"execute_command_with_callback\0").map_err(|e| {
                    FoundryLocalError::LibraryLoad(format!(
                        "Symbol 'execute_command_with_callback' not found: {e}"
                    ))
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
        let cmd = CString::new(command).map_err(|e| {
            FoundryLocalError::CommandExecution(format!("Invalid command string: {e}"))
        })?;

        let data_json = match params {
            Some(v) => serde_json::to_string(v)?,
            None => String::new(),
        };
        let data_cstr = CString::new(data_json.as_str()).map_err(|e| {
            FoundryLocalError::CommandExecution(format!("Invalid data string: {e}"))
        })?;

        let request = RequestBuffer {
            command: cmd.as_ptr(),
            command_length: cmd.as_bytes().len() as i32,
            data: data_cstr.as_ptr(),
            data_length: data_cstr.as_bytes().len() as i32,
        };

        let mut response = ResponseBuffer::new();

        unsafe {
            (self.execute_command)(&request, &mut response);
        }

        Self::process_response(&response)
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
        let cmd = CString::new(command).map_err(|e| {
            FoundryLocalError::CommandExecution(format!("Invalid command string: {e}"))
        })?;

        let data_json = match params {
            Some(v) => serde_json::to_string(v)?,
            None => String::new(),
        };
        let data_cstr = CString::new(data_json.as_str()).map_err(|e| {
            FoundryLocalError::CommandExecution(format!("Invalid data string: {e}"))
        })?;

        let request = RequestBuffer {
            command: cmd.as_ptr(),
            command_length: cmd.as_bytes().len() as i32,
            data: data_cstr.as_ptr(),
            data_length: data_cstr.as_bytes().len() as i32,
        };

        let mut response = ResponseBuffer::new();

        // Box the closure so we can pass a stable pointer through FFI.
        let mut boxed: Box<dyn FnMut(&str)> = Box::new(|chunk: &str| callback(chunk));
        let user_data = &mut boxed as *mut Box<dyn FnMut(&str)> as *mut std::ffi::c_void;

        unsafe {
            (self.execute_command_with_callback)(
                &request,
                &mut response,
                streaming_trampoline,
                user_data,
            );
        }

        Self::process_response(&response)
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
        tokio::task::spawn_blocking(move || {
            this.execute_command(&command, params.as_ref())
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution(format!("task join error: {e}")))?
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
        .map_err(|e| FoundryLocalError::CommandExecution(format!("task join error: {e}")))?
    }

    /// Async streaming variant that bridges the FFI callback into a
    /// [`tokio::sync::mpsc`] channel.
    ///
    /// Returns a `Receiver<String>` that yields each chunk as it arrives.
    /// The FFI call runs on a dedicated blocking thread; the receiver can
    /// be wrapped with [`tokio_stream::wrappers::ReceiverStream`] to get a
    /// `Stream`.
    pub async fn execute_command_streaming_channel(
        self: &Arc<Self>,
        command: String,
        params: Option<Value>,
    ) -> Result<(tokio::sync::mpsc::UnboundedReceiver<String>, tokio::task::JoinHandle<Result<String>>)> {
        let (tx, rx) = tokio::sync::mpsc::unbounded_channel::<String>();
        let this = Arc::clone(self);

        let handle = tokio::task::spawn_blocking(move || {
            this.execute_command_streaming(&command, params.as_ref(), move |chunk: &str| {
                // Ignore send errors — the receiver was dropped.
                let _ = tx.send(chunk.to_owned());
            })
        });

        Ok((rx, handle))
    }

    /// Read the response buffer, free the native memory, and return the data
    /// string or raise an error.
    fn process_response(response: &ResponseBuffer) -> Result<String> {
        // Extract strings from the native pointers before freeing them.
        let error_str = if !response.error.is_null() && response.error_length > 0 {
            Some(unsafe {
                let slice =
                    std::slice::from_raw_parts(response.error, response.error_length as usize);
                String::from_utf8_lossy(slice).into_owned()
            })
        } else {
            None
        };

        let data_str = if !response.data.is_null() && response.data_length > 0 {
            Some(unsafe {
                let slice =
                    std::slice::from_raw_parts(response.data, response.data_length as usize);
                String::from_utf8_lossy(slice).into_owned()
            })
        } else {
            None
        };

        // Free the heap-allocated response buffers (matches JS koffi.free()
        // and C# Marshal.FreeHGlobal() behaviour).
        unsafe {
            free_native_buffer(response.data);
            free_native_buffer(response.error);
        }

        // Return error or data.
        if let Some(err) = error_str {
            Err(FoundryLocalError::CommandExecution(err))
        } else {
            Ok(data_str.unwrap_or_default())
        }
    }

    /// Resolve the path to the native core shared library.
    fn resolve_library_path(config: &Configuration) -> Result<PathBuf> {
        // 1. Explicit path from configuration.
        if let Some(dir) = config.params.get("FoundryLocalCorePath") {
            let p = Path::new(dir).join(CORE_LIB_NAME);
            if p.exists() {
                return Ok(p);
            }
            // If the config value is the full path to the library itself.
            let p = Path::new(dir);
            if p.exists() && p.is_file() {
                return Ok(p.to_path_buf());
            }
        }

        // 2. Compile-time environment variable set by build.rs.
        if let Some(dir) = option_env!("FOUNDRY_NATIVE_DIR") {
            let p = Path::new(dir).join(CORE_LIB_NAME);
            if p.exists() {
                return Ok(p);
            }
        }

        // 3. Runtime environment variable (user override).
        if let Ok(dir) = std::env::var("FOUNDRY_NATIVE_DIR") {
            let p = Path::new(&dir).join(CORE_LIB_NAME);
            if p.exists() {
                return Ok(p);
            }
        }

        // 4. Next to the running executable.
        if let Ok(exe) = std::env::current_exe() {
            if let Some(dir) = exe.parent() {
                let p = dir.join(CORE_LIB_NAME);
                if p.exists() {
                    return Ok(p);
                }
            }
        }

        Err(FoundryLocalError::LibraryLoad(format!(
            "Could not locate native library '{CORE_LIB_NAME}'. \
             Set the FoundryLocalCorePath config option or the FOUNDRY_NATIVE_DIR \
             environment variable."
        )))
    }

    /// On Windows, pre-load runtime dependencies so the core library can
    /// resolve them.
    #[cfg(target_os = "windows")]
    fn load_windows_dependencies(core_lib_path: &Path) -> Result<Vec<Library>> {
        let dir = core_lib_path
            .parent()
            .unwrap_or_else(|| Path::new("."));

        let mut libs = Vec::new();

        // Load WinML bootstrap if present.
        let bootstrap = dir.join("Microsoft.WindowsAppRuntime.Bootstrap.dll");
        if bootstrap.exists() {
            if let Ok(lib) = unsafe { Library::new(&bootstrap) } {
                libs.push(lib);
            }
        }

        for dep in &["onnxruntime.dll", "onnxruntime-genai.dll"] {
            let dep_path = dir.join(dep);
            if dep_path.exists() {
                let lib = unsafe {
                    Library::new(&dep_path).map_err(|e| {
                        FoundryLocalError::LibraryLoad(format!(
                            "Failed to load dependency {dep}: {e}"
                        ))
                    })?
                };
                libs.push(lib);
            }
        }

        Ok(libs)
    }
}
