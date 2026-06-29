//! Helpers for constructing and reading native `flItem`s.
//!
//! The OpenAI facade and live-audio session build TEXT (`OPENAI_JSON`), AUDIO,
//! and BYTES items, and read TEXT items back out of responses / streamed queues.

use std::os::raw::c_char;
use std::ptr;

use super::api::{cstr_to_string, to_cstring, Api};
use super::ffi::*;
use crate::error::Result;

/// Create a TEXT item with the given subtype. The native layer copies the text.
pub(crate) fn make_text_item(
    api: &Api,
    text: &str,
    item_type: flTextItemType,
) -> Result<*mut flItem> {
    // Convert before Create so a NUL-conversion error can't leak the item.
    let c = to_cstring(text)?;
    let mut item: *mut flItem = ptr::null_mut();
    api.check(unsafe { (api.item_api().Create)(FOUNDRY_LOCAL_ITEM_TEXT, &mut item) })?;
    let data = flTextData {
        version: FOUNDRY_LOCAL_API_VERSION,
        text: c.as_ptr(),
        r#type: item_type,
    };
    // SAFETY: `item` is a valid TEXT item; the native call copies the string.
    let status = unsafe { (api.item_api().SetText)(item, &data) };
    if let Err(e) = api.check(status) {
        unsafe { (api.item_api().Item_Release)(item) };
        return Err(e);
    }
    Ok(item)
}

/// Create a TEXT item carrying an opaque OpenAI REST JSON payload.
pub(crate) fn make_openai_json_item(api: &Api, json: &str) -> Result<*mut flItem> {
    make_text_item(api, json, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON)
}

/// Create a byte-backed AUDIO item describing a PCM format (used as the live
/// audio format descriptor) or carrying audio bytes. The native layer copies
/// the data, so the caller's buffer need not outlive the call.
pub(crate) fn make_audio_item(
    api: &Api,
    data: &[u8],
    format: Option<&str>,
    sample_rate: i32,
    channels: i32,
) -> Result<*mut flItem> {
    // Convert before Create so a NUL-conversion error can't leak the item.
    let format_c = match format {
        Some(f) => Some(to_cstring(f)?),
        None => None,
    };

    let mut item: *mut flItem = ptr::null_mut();
    api.check(unsafe { (api.item_api().Create)(FOUNDRY_LOCAL_ITEM_AUDIO, &mut item) })?;

    // Like SetBytes, SetAudio does not copy the sample buffer — it borrows the
    // pointer (and frees it via the deleter when one is supplied). Transfer an
    // owned heap allocation so the buffer outlives this call; the format string
    // is copied natively, so a transient CString is fine.
    let (data_ptr, len, deleter): (*mut u8, usize, flAudioDataDeleter) = if data.is_empty() {
        (ptr::null_mut(), 0, None)
    } else {
        let boxed: Box<[u8]> = data.to_vec().into_boxed_slice();
        let len = boxed.len();
        (
            Box::into_raw(boxed) as *mut u8,
            len,
            Some(rust_audio_deleter),
        )
    };

    let audio = flAudioData {
        version: FOUNDRY_LOCAL_API_VERSION,
        data: data_ptr as *const std::ffi::c_void,
        mutable_data: data_ptr as *mut std::ffi::c_void,
        data_size: len,
        format: format_c.as_ref().map_or(ptr::null(), |c| c.as_ptr()),
        uri: ptr::null(),
        sample_rate,
        channels,
        deleter,
        deleter_user_data: ptr::null_mut(),
    };
    // SAFETY: `item` is a valid AUDIO item. On success the item owns `data_ptr`
    // (freed via the deleter); on failure we reclaim it here to avoid a leak.
    let status = unsafe { (api.item_api().SetAudio)(item, &audio) };
    if let Err(e) = api.check(status) {
        unsafe {
            if !data_ptr.is_null() {
                drop(Box::from_raw(ptr::slice_from_raw_parts_mut(data_ptr, len)));
            }
            (api.item_api().Item_Release)(item);
        }
        return Err(e);
    }
    Ok(item)
}

/// Deleter that reclaims a Rust-allocated `Box<[u8]>` owned by an AUDIO item.
/// Mirrors [`rust_bytes_deleter`]; see its docs for the ownership contract.
unsafe extern "C" fn rust_audio_deleter(
    data: *const flAudioData,
    _user_data: *mut std::ffi::c_void,
) {
    if data.is_null() {
        return;
    }
    let d = &*data;
    if !d.mutable_data.is_null() && d.data_size > 0 {
        let slice = ptr::slice_from_raw_parts_mut(d.mutable_data as *mut u8, d.data_size);
        drop(Box::from_raw(slice));
    }
}

/// Deleter that reclaims a Rust-allocated `Box<[u8]>` owned by a BYTES item.
///
/// The native item calls this on destruction. `mutable_data` is the pointer we
/// handed over via `Box::into_raw`, and `data_size` is its length; together they
/// reconstruct the boxed slice so it is dropped exactly once.
unsafe extern "C" fn rust_bytes_deleter(
    data: *const flBytesData,
    _user_data: *mut std::ffi::c_void,
) {
    if data.is_null() {
        return;
    }
    let d = &*data;
    if !d.mutable_data.is_null() && d.data_size > 0 {
        let slice = ptr::slice_from_raw_parts_mut(d.mutable_data as *mut u8, d.data_size);
        drop(Box::from_raw(slice));
    }
}

/// Create a BYTES item tagged with the given originating item type (e.g. AUDIO
/// for raw PCM chunks pushed into a live session).
///
/// The native `SetBytes` does **not** copy — it stores the pointer and (when a
/// deleter is supplied) takes ownership of the buffer, freeing it via the
/// deleter when the item is destroyed. The item may be consumed asynchronously
/// (e.g. drained from an `ItemQueue` by a streaming worker long after this
/// returns), so the buffer must outlive this call. We therefore transfer an
/// owned heap allocation to the item rather than lending a caller buffer.
pub(crate) fn make_bytes_item(
    api: &Api,
    data: &[u8],
    item_type: flItemType,
) -> Result<*mut flItem> {
    let mut item: *mut flItem = ptr::null_mut();
    api.check(unsafe { (api.item_api().Create)(FOUNDRY_LOCAL_ITEM_BYTES, &mut item) })?;

    if data.is_empty() {
        let bytes = flBytesData {
            version: FOUNDRY_LOCAL_API_VERSION,
            item_type,
            data: ptr::null(),
            mutable_data: ptr::null_mut(),
            data_size: 0,
            deleter: None,
            deleter_user_data: ptr::null_mut(),
        };
        let status = unsafe { (api.item_api().SetBytes)(item, &bytes) };
        if let Err(e) = api.check(status) {
            unsafe { (api.item_api().Item_Release)(item) };
            return Err(e);
        }
        return Ok(item);
    }

    // Transfer an owned copy to the item. `into_boxed_slice` guarantees
    // capacity == len, so the deleter can reconstruct it from (ptr, len).
    let boxed: Box<[u8]> = data.to_vec().into_boxed_slice();
    let len = boxed.len();
    let raw = Box::into_raw(boxed) as *mut u8;

    let bytes = flBytesData {
        version: FOUNDRY_LOCAL_API_VERSION,
        item_type,
        data: raw as *const std::ffi::c_void,
        mutable_data: raw as *mut std::ffi::c_void,
        data_size: len,
        deleter: Some(rust_bytes_deleter),
        deleter_user_data: ptr::null_mut(),
    };

    // SAFETY: `item` is a valid BYTES item. On success the item owns `raw` and
    // frees it via the deleter; on failure SetBytesData was not applied, so we
    // reclaim and drop the box here to avoid leaking it.
    let status = unsafe { (api.item_api().SetBytes)(item, &bytes) };
    if let Err(e) = api.check(status) {
        unsafe {
            let slice = ptr::slice_from_raw_parts_mut(raw, len);
            drop(Box::from_raw(slice));
            (api.item_api().Item_Release)(item);
        }
        return Err(e);
    }
    Ok(item)
}

/// Read the text of a TEXT item. Returns `None` for null/non-text items.
///
/// # Safety
/// `item` must be null or a valid item pointer alive for the duration of this call.
pub(crate) unsafe fn read_text_item(api: &Api, item: *const flItem) -> Option<String> {
    if item.is_null() {
        return None;
    }
    if (api.item_api().GetType)(item) != FOUNDRY_LOCAL_ITEM_TEXT {
        return None;
    }
    let mut data = flTextData {
        version: FOUNDRY_LOCAL_API_VERSION,
        text: ptr::null::<c_char>(),
        r#type: FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT,
    };
    if api
        .check((api.item_api().GetText)(item, &mut data))
        .is_err()
    {
        return None;
    }
    cstr_to_string(data.text)
}

/// Text + timing read from a SPEECH_SEGMENT item (output-only).
pub(crate) struct SpeechSegmentText {
    pub text: String,
    pub is_final: bool,
    pub start_time_s: Option<f64>,
    pub end_time_s: Option<f64>,
}

/// Convert a native millisecond field to seconds, mapping the UNSET sentinel to `None`.
fn duration_ms_to_seconds(ms: i64) -> Option<f64> {
    if ms == FOUNDRY_LOCAL_DURATION_UNSET {
        None
    } else {
        Some(ms as f64 / 1000.0)
    }
}

/// Read a SPEECH_SEGMENT item (output-only). Returns `None` for null/other items.
///
/// # Safety
/// `item` must be null or a valid item pointer alive for the duration of this call.
pub(crate) unsafe fn read_speech_segment(
    api: &Api,
    item: *const flItem,
) -> Option<SpeechSegmentText> {
    if item.is_null() || (api.item_api().GetType)(item) != FOUNDRY_LOCAL_ITEM_SPEECH_SEGMENT {
        return None;
    }
    let mut data = flSpeechSegmentData {
        version: FOUNDRY_LOCAL_API_VERSION,
        kind: FOUNDRY_LOCAL_SPEECH_SEGMENT_NONE,
        text: ptr::null::<c_char>(),
        start_time_ms: FOUNDRY_LOCAL_DURATION_UNSET,
        end_time_ms: FOUNDRY_LOCAL_DURATION_UNSET,
        utterance_start: false,
        words: ptr::null::<flSpeechWord>(),
        words_count: 0,
        language: ptr::null::<c_char>(),
    };
    if api
        .check((api.item_api().GetSpeechSegment)(item, &mut data))
        .is_err()
    {
        return None;
    }
    Some(SpeechSegmentText {
        text: cstr_to_string(data.text).unwrap_or_default(),
        // PARTIAL is an interim hypothesis; FINAL (and NONE entries) are stable.
        is_final: data.kind == FOUNDRY_LOCAL_SPEECH_SEGMENT_FINAL,
        start_time_s: duration_ms_to_seconds(data.start_time_ms),
        end_time_s: duration_ms_to_seconds(data.end_time_ms),
    })
}

/// Read the concatenated transcript of a SPEECH_RESULT item (output-only).
/// Returns `None` for null/other items.
///
/// # Safety
/// `item` must be null or a valid item pointer alive for the duration of this call.
pub(crate) unsafe fn read_speech_result_text(api: &Api, item: *const flItem) -> Option<String> {
    if item.is_null() || (api.item_api().GetType)(item) != FOUNDRY_LOCAL_ITEM_SPEECH_RESULT {
        return None;
    }
    let mut data = flSpeechResultData {
        version: FOUNDRY_LOCAL_API_VERSION,
        text: ptr::null::<c_char>(),
        language: ptr::null::<c_char>(),
        duration_ms: FOUNDRY_LOCAL_DURATION_UNSET,
        segments: ptr::null::<*const flItem>(),
        segments_count: 0,
    };
    if api
        .check((api.item_api().GetSpeechResult)(item, &mut data))
        .is_err()
    {
        return None;
    }
    cstr_to_string(data.text)
}
