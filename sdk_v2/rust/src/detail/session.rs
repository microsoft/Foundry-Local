//! Safe wrappers over native `flRequest`, `flResponse`, and `flSession`, plus the
//! OpenAI-JSON request/response and streaming bridges used by the OpenAI facade.

use std::os::raw::c_int;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::Arc;

use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};

use super::api::Api;
use super::ffi::*;
use super::items::{
    make_bytes_item, make_openai_json_item, read_speech_result_text, read_text_item,
};
use super::manager::NativeManager;
use super::native::NativeModel;
use crate::error::{FoundryLocalError, Result};

/// Per-item transform applied to streamed TEXT payloads before they are emitted.
pub(crate) type StreamTransform = Box<dyn Fn(String) -> Option<String> + Send>;

// ── Request ──────────────────────────────────────────────────────────────────

pub(crate) struct NativeRequest {
    api: Arc<Api>,
    ptr: *mut flRequest,
}

impl NativeRequest {
    pub(crate) fn new(api: Arc<Api>) -> Result<Self> {
        let mut ptr: *mut flRequest = ptr::null_mut();
        api.check(unsafe { (api.inference_api().Request_Create)(&mut ptr) })?;
        Ok(Self { api, ptr })
    }

    /// Add an item, transferring ownership to the request.
    pub(crate) fn add_item(&self, item: *mut flItem, take_ownership: bool) -> Result<()> {
        let status =
            unsafe { (self.api.inference_api().Request_AddItem)(self.ptr, item, take_ownership) };
        self.api.check(status)
    }
}

impl Drop for NativeRequest {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { (self.api.inference_api().Request_Release)(self.ptr) };
            self.ptr = ptr::null_mut();
        }
    }
}

// ── Response ─────────────────────────────────────────────────────────────────

pub(crate) struct NativeResponse {
    api: Arc<Api>,
    ptr: *mut flResponse,
}

impl NativeResponse {
    pub(crate) fn item_count(&self) -> usize {
        unsafe { (self.api.inference_api().Response_GetItemCount)(self.ptr) }
    }

    /// Read the text payload of the response item at `idx` (if it is a TEXT item).
    pub(crate) fn item_text(&self, idx: usize) -> Option<String> {
        let mut item: *const flItem = ptr::null();
        let status =
            unsafe { (self.api.inference_api().Response_GetItem)(self.ptr, idx, &mut item) };
        if self.api.check(status).is_err() {
            return None;
        }
        unsafe { read_text_item(&self.api, item) }
    }

    /// Read the transcript of the response item at `idx` (if it is a SPEECH_RESULT item).
    pub(crate) fn item_speech_result_text(&self, idx: usize) -> Option<String> {
        let mut item: *const flItem = ptr::null();
        let status =
            unsafe { (self.api.inference_api().Response_GetItem)(self.ptr, idx, &mut item) };
        if self.api.check(status).is_err() {
            return None;
        }
        unsafe { read_speech_result_text(&self.api, item) }
    }
}

impl Drop for NativeResponse {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { (self.api.inference_api().Response_Release)(self.ptr) };
            self.ptr = ptr::null_mut();
        }
    }
}

// ── ItemQueue ────────────────────────────────────────────────────────────────

/// Owning wrapper around a native input `flItemQueue`.
///
/// In the C ABI an `ItemQueue` *is* an `Item` (same pointer, castable), so it can
/// be added to a request directly and released via `Item_Release`.
pub(crate) struct NativeItemQueue {
    api: Arc<Api>,
    ptr: *mut flItemQueue,
}

// SAFETY: the native item queue is documented as thread-safe (multi-producer /
// multi-consumer); pushing from any thread is supported.
unsafe impl Send for NativeItemQueue {}
unsafe impl Sync for NativeItemQueue {}

impl NativeItemQueue {
    pub(crate) fn new(api: Arc<Api>) -> Result<Self> {
        let mut ptr: *mut flItemQueue = ptr::null_mut();
        api.check(unsafe { (api.item_api().ItemQueue_Create)(&mut ptr) })?;
        Ok(Self { api, ptr })
    }

    /// The queue as an `flItem*` (for adding to a request).
    pub(crate) fn as_item_ptr(&self) -> *mut flItem {
        self.ptr as *mut flItem
    }

    /// Push an item, transferring ownership into the queue.
    ///
    /// `ItemQueue_Push` takes ownership of `item` *unconditionally* for a
    /// non-null queue and item: the native side moves the raw pointer into a
    /// `unique_ptr` before enqueuing, so even if enqueuing fails the item is
    /// already (or will be) freed. Callers must therefore **not** release `item`
    /// on a returned error — doing so would double-free.
    pub(crate) fn push_item(&self, item: *mut flItem) -> Result<()> {
        self.api
            .check(unsafe { (self.api.item_api().ItemQueue_Push)(self.ptr, item) })
    }

    /// Create a BYTES item from `data` and push it into the queue.
    pub(crate) fn push_bytes(&self, data: &[u8], item_type: flItemType) -> Result<()> {
        let item = make_bytes_item(&self.api, data, item_type)?;
        // `push_item` consumes `item` on every path (see its docs); do not
        // release it here on error.
        self.push_item(item)
    }

    /// Signal that no more items will be pushed.
    pub(crate) fn mark_finished(&self) {
        unsafe { (self.api.item_api().ItemQueue_MarkFinished)(self.ptr) };
    }
}

impl Drop for NativeItemQueue {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            // The queue is an Item; release via the polymorphic Item destructor.
            unsafe { (self.api.item_api().Item_Release)(self.ptr as *mut flItem) };
            self.ptr = ptr::null_mut();
        }
    }
}

// ── Session ──────────────────────────────────────────────────────────────────

pub(crate) struct NativeSession {
    pub(crate) api: Arc<Api>,
    ptr: *mut flSession,
    /// Keeps the native manager (which owns the model this session was created
    /// from) alive for the session's lifetime; never dereferenced here.
    _manager: Arc<NativeManager>,
}

// SAFETY: a session is used from a single worker at a time; the native layer is
// thread-safe for the create/process/release lifecycle used here. The owning
// native manager is kept alive via `_manager`.
unsafe impl Send for NativeSession {}
unsafe impl Sync for NativeSession {}

impl NativeSession {
    /// Create a session bound to the given model variant.
    pub(crate) fn create(model: &NativeModel) -> Result<Self> {
        let api = Arc::clone(&model.api);
        let mut ptr: *mut flSession = ptr::null_mut();
        api.check(unsafe { (api.inference_api().Session_Create)(model.ptr, &mut ptr) })?;
        Ok(Self {
            api,
            ptr,
            _manager: model.manager(),
        })
    }

    pub(crate) fn set_streaming_callback(
        &self,
        callback: flStreamingCallback,
        user_data: *mut std::ffi::c_void,
    ) -> Result<()> {
        let status = unsafe {
            (self.api.inference_api().Session_SetStreamingCallback)(self.ptr, callback, user_data)
        };
        self.api.check(status)
    }

    pub(crate) fn process_request(&self, request: &NativeRequest) -> Result<NativeResponse> {
        let mut resp: *mut flResponse = ptr::null_mut();
        let status = unsafe {
            (self.api.inference_api().Session_ProcessRequest)(self.ptr, request.ptr, &mut resp)
        };
        self.api.check(status)?;
        Ok(NativeResponse {
            api: Arc::clone(&self.api),
            ptr: resp,
        })
    }

    /// Run a non-streaming OpenAI-JSON request and return the response payload.
    ///
    /// The request JSON is sent as a single `OPENAI_JSON` TEXT item; the response
    /// payload is the text of the first response item. Blocking.
    pub(crate) fn run_openai_json(&self, request_json: &str) -> Result<String> {
        let request = NativeRequest::new(Arc::clone(&self.api))?;
        let item = make_openai_json_item(&self.api, request_json)?;
        request.add_item(item, true)?;
        let response = self.process_request(&request)?;
        if response.item_count() == 0 {
            return Err(FoundryLocalError::CommandExecution {
                reason: "Native response contained no items".into(),
            });
        }
        response
            .item_text(0)
            .ok_or_else(|| FoundryLocalError::CommandExecution {
                reason: "Native response item was not readable text".into(),
            })
    }
}

impl Drop for NativeSession {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { (self.api.inference_api().Session_Release)(self.ptr) };
            self.ptr = ptr::null_mut();
        }
    }
}

// ── Streaming bridge ─────────────────────────────────────────────────────────

struct StreamCtx {
    api: Arc<Api>,
    tx: UnboundedSender<Result<String>>,
    transform: StreamTransform,
}

unsafe extern "C" fn stream_trampoline(
    data: flStreamingCallbackData,
    user_data: *mut std::ffi::c_void,
) -> c_int {
    if user_data.is_null() {
        return 0;
    }
    let result = catch_unwind(AssertUnwindSafe(|| {
        let ctx = &*(user_data as *const StreamCtx);
        let queue = data.item_queue;
        if queue.is_null() {
            return 0;
        }
        let item_api = ctx.api.item_api();
        loop {
            let mut item: *mut flItem = ptr::null_mut();
            let popped = (item_api.ItemQueue_TryPop)(queue, &mut item);
            if !popped {
                break;
            }
            if item.is_null() {
                continue;
            }
            // Ownership of `item` transferred to us — read then release.
            let text = read_text_item(&ctx.api, item);
            (item_api.Item_Release)(item);
            if let Some(text) = text {
                if let Some(transformed) = (ctx.transform)(text) {
                    if ctx.tx.send(Ok(transformed)).is_err() {
                        return 1; // receiver dropped — cancel generation
                    }
                }
            }
        }
        0
    }));
    result.unwrap_or(1)
}

/// Run a streaming OpenAI-JSON request, returning a channel of transformed
/// per-item TEXT payloads.
///
/// `transform` is applied to each streamed item's text (return `None` to skip
/// an item). The session is created and processed on a blocking worker thread;
/// the channel closes when generation completes or errors.
pub(crate) fn run_openai_json_streaming(
    session: NativeSession,
    request_json: String,
    transform: StreamTransform,
) -> UnboundedReceiver<Result<String>> {
    let (tx, rx) = tokio::sync::mpsc::unbounded_channel::<Result<String>>();

    tokio::task::spawn_blocking(move || {
        let ctx = Box::new(StreamCtx {
            api: Arc::clone(&session.api),
            tx: tx.clone(),
            transform,
        });
        let ctx_ptr = &*ctx as *const StreamCtx as *mut std::ffi::c_void;

        if let Err(e) = session.set_streaming_callback(Some(stream_trampoline), ctx_ptr) {
            let _ = tx.send(Err(e));
            return;
        }

        let run = (|| -> Result<()> {
            let request = NativeRequest::new(Arc::clone(&session.api))?;
            let item = make_openai_json_item(&session.api, &request_json)?;
            request.add_item(item, true)?;
            let _response = session.process_request(&request)?;
            Ok(())
        })();
        if let Err(e) = run {
            let _ = tx.send(Err(e));
        }

        // Uninstall the callback before the context/session are dropped.
        let _ = session.set_streaming_callback(None, ptr::null_mut());
        drop(ctx);
        drop(session);
    });

    rx
}
