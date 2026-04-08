//! Live audio transcription streaming session.
//!
//! Provides real-time audio streaming ASR (Automatic Speech Recognition).
//! Audio data from a microphone (or other source) is pushed in as PCM chunks
//! and transcription results are returned as an async [`Stream`](futures_core::Stream).
//!
//! # Example
//!
//! ```ignore
//! let audio_client = model.create_audio_client();
//! let mut session = audio_client.create_live_transcription_session();
//! session.settings.sample_rate = 16000;
//! session.settings.channels = 1;
//! session.settings.language = Some("en".into());
//!
//! session.start().await?;
//!
//! // Push audio from microphone callback
//! session.append(&pcm_bytes).await?;
//!
//! // Read results as async stream
//! use tokio_stream::StreamExt;
//! let mut stream = session.get_transcription_stream()?;
//! while let Some(result) = stream.next().await {
//!     let result = result?;
//!     print!("{}", result.text);
//! }
//!
//! session.stop().await?;
//! ```

use std::pin::Pin;
use std::sync::Arc;
use std::task::{Context, Poll};

use serde_json::json;

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};

// ── Types ────────────────────────────────────────────────────────────────────

/// Audio format settings for a live transcription session.
///
/// Must be configured before calling [`LiveAudioTranscriptionSession::start`].
/// Settings are frozen once the session starts.
#[derive(Debug, Clone)]
pub struct LiveAudioTranscriptionOptions {
    /// PCM sample rate in Hz. Default: 16000.
    pub sample_rate: i32,
    /// Number of audio channels. Default: 1 (mono).
    pub channels: i32,
    /// Number of bits per audio sample. Default: 16.
    pub bits_per_sample: i32,
    /// Optional BCP-47 language hint (e.g., `"en"`, `"zh"`).
    pub language: Option<String>,
    /// Maximum number of audio chunks buffered in the internal push queue.
    /// If the queue is full, [`LiveAudioTranscriptionSession::append`] will
    /// wait asynchronously.
    /// Default: 100 (~3 seconds of audio at typical chunk sizes).
    pub push_queue_capacity: usize,
}

impl Default for LiveAudioTranscriptionOptions {
    fn default() -> Self {
        Self {
            sample_rate: 16000,
            channels: 1,
            bits_per_sample: 16,
            language: None,
            push_queue_capacity: 100,
        }
    }
}

/// Internal raw deserialization target matching the native core's JSON format.
#[derive(Debug, Clone, serde::Deserialize)]
struct LiveAudioTranscriptionRaw {
    #[serde(default)]
    is_final: bool,
    #[serde(default)]
    text: String,
    start_time: Option<f64>,
    end_time: Option<f64>,
}

/// Transcription result from a live audio streaming session.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct LiveAudioTranscriptionResponse {
    /// The transcribed text.
    pub text: String,
    /// Same as `text` — provided for OpenAI Realtime API compatibility.
    pub transcript: String,
    /// Whether this is a final or partial (interim) result.
    /// Nemotron models always return `true`; other models may return `false`
    /// for interim hypotheses that will be replaced by a subsequent final result.
    pub is_final: bool,
    /// Start time offset of this segment in the audio stream (seconds).
    pub start_time: Option<f64>,
    /// End time offset of this segment in the audio stream (seconds).
    pub end_time: Option<f64>,
}

impl LiveAudioTranscriptionResponse {
    /// Parse a transcription response from the native core's JSON format.
    pub fn from_json(json: &str) -> Result<Self> {
        let raw: LiveAudioTranscriptionRaw = serde_json::from_str(json)?;
        Ok(Self::from_raw(raw))
    }

    fn from_raw(raw: LiveAudioTranscriptionRaw) -> Self {
        Self {
            transcript: raw.text.clone(),
            text: raw.text,
            is_final: raw.is_final,
            start_time: raw.start_time,
            end_time: raw.end_time,
        }
    }
}

/// Structured error response from the native core.
#[derive(Debug, Clone, serde::Deserialize)]
pub struct CoreErrorResponse {
    /// Error code (e.g. `"ASR_SESSION_NOT_FOUND"`).
    pub code: String,
    /// Human-readable error message.
    pub message: String,
    /// Whether this error is transient (retryable).
    #[serde(rename = "isTransient", default)]
    pub is_transient: bool,
}

impl CoreErrorResponse {
    /// Attempt to parse a native error string as structured JSON.
    /// Returns `None` if the error is not valid JSON or doesn't match the schema.
    pub fn try_parse(error_string: &str) -> Option<Self> {
        serde_json::from_str(error_string).ok()
    }
}

// ── Stream type ──────────────────────────────────────────────────────────────

/// An async stream of [`LiveAudioTranscriptionResponse`] items.
///
/// Returned by [`LiveAudioTranscriptionSession::get_transcription_stream`].
/// Implements [`futures_core::Stream`].
pub struct LiveAudioTranscriptionStream {
    rx: tokio::sync::mpsc::UnboundedReceiver<Result<LiveAudioTranscriptionResponse>>,
}

impl Unpin for LiveAudioTranscriptionStream {}

impl futures_core::Stream for LiveAudioTranscriptionStream {
    type Item = Result<LiveAudioTranscriptionResponse>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.rx.poll_recv(cx)
    }
}

// ── Session state ────────────────────────────────────────────────────────────

struct SessionState {
    session_handle: Option<String>,
    started: bool,
    stopped: bool,
    push_tx: Option<tokio::sync::mpsc::Sender<Vec<u8>>>,
    output_tx: Option<tokio::sync::mpsc::UnboundedSender<Result<LiveAudioTranscriptionResponse>>>,
    output_rx: Option<
        tokio::sync::mpsc::UnboundedReceiver<Result<LiveAudioTranscriptionResponse>>,
    >,
    push_loop_handle: Option<tokio::task::JoinHandle<()>>,
    active_settings: Option<LiveAudioTranscriptionOptions>,
}

impl SessionState {
    fn new() -> Self {
        Self {
            session_handle: None,
            started: false,
            stopped: false,
            push_tx: None,
            output_tx: None,
            output_rx: None,
            push_loop_handle: None,
            active_settings: None,
        }
    }
}

// ── Session ──────────────────────────────────────────────────────────────────

/// Session for real-time audio streaming ASR (Automatic Speech Recognition).
///
/// Audio data from a microphone (or other source) is pushed in as PCM chunks
/// via [`append`](Self::append), and transcription results are returned as an
/// async [`Stream`](futures_core::Stream) via
/// [`get_transcription_stream`](Self::get_transcription_stream).
///
/// Created via [`AudioClient::create_live_transcription_session`](super::AudioClient::create_live_transcription_session).
///
/// # Thread safety
///
/// [`append`](Self::append) can be called from any thread (including
/// high-frequency audio callbacks). Pushes are internally serialized via a
/// bounded channel to prevent unbounded memory growth and ensure ordering.
pub struct LiveAudioTranscriptionSession {
    model_id: String,
    core: Arc<CoreInterop>,
    /// Audio format settings. Must be configured before calling [`start`](Self::start).
    /// Settings are frozen once the session starts.
    pub settings: LiveAudioTranscriptionOptions,
    state: tokio::sync::Mutex<SessionState>,
}

impl LiveAudioTranscriptionSession {
    pub(crate) fn new(model_id: &str, core: Arc<CoreInterop>) -> Self {
        Self {
            model_id: model_id.to_owned(),
            core,
            settings: LiveAudioTranscriptionOptions::default(),
            state: tokio::sync::Mutex::new(SessionState::new()),
        }
    }

    /// Start a real-time audio streaming session.
    ///
    /// Must be called before [`append`](Self::append) or
    /// [`get_transcription_stream`](Self::get_transcription_stream).
    /// Settings are frozen after this call.
    pub async fn start(&self) -> Result<()> {
        let mut state = self.state.lock().await;

        if state.started {
            return Err(FoundryLocalError::Validation {
                reason: "Streaming session already started. Call stop() first.".into(),
            });
        }

        // Freeze settings
        let active_settings = self.settings.clone();

        // Create output channel (unbounded — only the push loop writes)
        let (output_tx, output_rx) =
            tokio::sync::mpsc::unbounded_channel::<Result<LiveAudioTranscriptionResponse>>();

        // Create push channel (bounded — backpressure if native core is slower than real-time)
        let (push_tx, push_rx) =
            tokio::sync::mpsc::channel::<Vec<u8>>(active_settings.push_queue_capacity);

        // Build request params
        let mut params = serde_json::Map::new();
        params.insert("Model".into(), json!(self.model_id));
        params.insert(
            "SampleRate".into(),
            json!(active_settings.sample_rate.to_string()),
        );
        params.insert(
            "Channels".into(),
            json!(active_settings.channels.to_string()),
        );
        params.insert(
            "BitsPerSample".into(),
            json!(active_settings.bits_per_sample.to_string()),
        );
        if let Some(ref lang) = active_settings.language {
            params.insert("Language".into(), json!(lang));
        }

        let request = json!({ "Params": serde_json::Value::Object(params) });

        // Start the native audio stream session (synchronous FFI on blocking thread)
        let core = Arc::clone(&self.core);
        let session_handle = tokio::task::spawn_blocking(move || {
            core.execute_command("audio_stream_start", Some(&request))
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("Start audio stream task join error: {e}"),
        })??;

        if session_handle.is_empty() {
            return Err(FoundryLocalError::CommandExecution {
                reason: "Native core did not return a session handle.".into(),
            });
        }

        state.session_handle = Some(session_handle.clone());
        state.started = true;
        state.stopped = false;
        state.active_settings = Some(active_settings);

        // Spawn the push loop on a blocking thread
        let push_loop_core = Arc::clone(&self.core);
        let push_loop_output_tx = output_tx.clone();
        let push_loop_handle = tokio::task::spawn_blocking(move || {
            Self::push_loop(push_loop_core, session_handle, push_rx, push_loop_output_tx);
        });

        state.push_tx = Some(push_tx);
        state.output_tx = Some(output_tx);
        state.output_rx = Some(output_rx);
        state.push_loop_handle = Some(push_loop_handle);

        Ok(())
    }

    /// Push a chunk of raw PCM audio data to the streaming session.
    ///
    /// Can be called from any async context (including high-frequency audio
    /// callbacks when wrapped). Chunks are internally queued and serialized to
    /// the native core.
    ///
    /// The data is copied internally so the caller can reuse the buffer.
    pub async fn append(&self, pcm_data: &[u8]) -> Result<()> {
        let state = self.state.lock().await;

        if !state.started || state.stopped {
            return Err(FoundryLocalError::Validation {
                reason: "No active streaming session. Call start() first.".into(),
            });
        }

        let tx = state.push_tx.as_ref().ok_or_else(|| FoundryLocalError::Internal {
            reason: "Push channel missing".into(),
        })?;

        // Copy the data to avoid issues if the caller reuses the buffer
        tx.send(pcm_data.to_vec())
            .await
            .map_err(|_| FoundryLocalError::CommandExecution {
                reason: "Push channel closed — session may have been stopped".into(),
            })
    }

    /// Get the async stream of transcription results.
    ///
    /// Results arrive as the native ASR engine processes audio data.
    /// Can only be called once per session (the receiver is moved out).
    pub fn get_transcription_stream(&self) -> Result<LiveAudioTranscriptionStream> {
        // We need to try_lock to avoid blocking — but in practice this is
        // called from the same task that called start().
        let mut state = self.state.try_lock().map_err(|_| FoundryLocalError::Internal {
            reason: "Could not acquire session lock for get_transcription_stream".into(),
        })?;

        let rx = state
            .output_rx
            .take()
            .ok_or_else(|| FoundryLocalError::Validation {
                reason: "No active streaming session, or stream already taken. \
                         Call start() first and only call get_transcription_stream() once."
                    .into(),
            })?;

        Ok(LiveAudioTranscriptionStream { rx })
    }

    /// Signal end-of-audio and stop the streaming session.
    ///
    /// Any remaining buffered audio in the push queue will be drained to the
    /// native core first. Final results are delivered through the transcription
    /// stream before it completes.
    pub async fn stop(&self) -> Result<()> {
        let mut state = self.state.lock().await;

        if !state.started || state.stopped {
            return Ok(()); // already stopped or never started
        }

        state.stopped = true;

        // 1. Complete the push channel so the push loop drains remaining items
        state.push_tx.take();

        // 2. Wait for the push loop to finish draining
        if let Some(handle) = state.push_loop_handle.take() {
            let _ = handle.await;
        }

        // 3. Tell native core to flush and finalize
        let session_handle = state
            .session_handle
            .as_ref()
            .ok_or_else(|| FoundryLocalError::Internal {
                reason: "Session handle missing during stop".into(),
            })?
            .clone();

        let params = json!({
            "Params": {
                "SessionHandle": session_handle
            }
        });

        let core = Arc::clone(&self.core);
        let stop_result = tokio::task::spawn_blocking(move || {
            core.execute_command("audio_stream_stop", Some(&params))
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("Stop audio stream task join error: {e}"),
        })?;

        // Parse final transcription from stop response before completing the channel
        match &stop_result {
            Ok(data) if !data.is_empty() => {
                if let Ok(raw) = serde_json::from_str::<LiveAudioTranscriptionRaw>(data) {
                    if !raw.text.is_empty() {
                        if let Some(tx) = &state.output_tx {
                            let _ = tx.send(Ok(LiveAudioTranscriptionResponse::from_raw(raw)));
                        }
                    }
                }
            }
            _ => {}
        }

        // Complete the output channel
        state.output_tx.take();
        state.session_handle = None;
        state.started = false;

        // Propagate error if native stop failed
        stop_result?;

        Ok(())
    }

    /// Internal push loop — runs entirely on a blocking thread.
    ///
    /// Drains the push queue and sends chunks to the native core one at a time.
    /// Terminates the session on any native error.
    fn push_loop(
        core: Arc<CoreInterop>,
        session_handle: String,
        mut push_rx: tokio::sync::mpsc::Receiver<Vec<u8>>,
        output_tx: tokio::sync::mpsc::UnboundedSender<Result<LiveAudioTranscriptionResponse>>,
    ) {
        while let Some(audio_data) = push_rx.blocking_recv() {
            let params = json!({
                "Params": {
                    "SessionHandle": &session_handle
                }
            });

            let result =
                core.execute_command_with_binary("audio_stream_push", Some(&params), &audio_data);

            match result {
                Ok(data) if !data.is_empty() => {
                    match serde_json::from_str::<LiveAudioTranscriptionRaw>(&data) {
                        Ok(raw) if !raw.text.is_empty() => {
                            let response = LiveAudioTranscriptionResponse::from_raw(raw);
                            let _ = output_tx.send(Ok(response));
                        }
                        Ok(_) => {} // empty text — skip
                        Err(_) => {} // non-fatal parse error — skip
                    }
                }
                Ok(_) => {} // empty response — skip
                Err(e) => {
                    // Fatal error from native core — terminate push loop
                    let error_info = CoreErrorResponse::try_parse(&format!("{e}"));
                    let code = error_info
                        .as_ref()
                        .map(|ei| ei.code.as_str())
                        .unwrap_or("UNKNOWN");
                    let _ = output_tx.send(Err(FoundryLocalError::CommandExecution {
                        reason: format!("Push failed (code={code}): {e}"),
                    }));
                    return;
                }
            }
        }
        // push_rx closed = push channel completed = push loop exits naturally
    }
}

// ── Drop impl ────────────────────────────────────────────────────────────────

impl Drop for LiveAudioTranscriptionSession {
    fn drop(&mut self) {
        if let Ok(mut state) = self.state.try_lock() {
            // Close push channel to unblock the push loop
            state.push_tx.take();
            state.output_tx.take();

            // Best-effort native cleanup: call audio_stream_stop synchronously
            // to prevent native session leaks. This is critical for long-running
            // processes where users may forget to call stop().
            if state.started && !state.stopped {
                if let Some(ref handle) = state.session_handle {
                    let params = serde_json::json!({
                        "Params": { "SessionHandle": handle }
                    });
                    // Synchronous FFI call — safe from Drop since execute_command
                    // is a blocking call that doesn't require an async runtime.
                    let _ = self
                        .core
                        .execute_command("audio_stream_stop", Some(&params));
                }
                state.session_handle = None;
                state.started = false;
                state.stopped = true;
            }
        }
    }
}

// ── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    // --- LiveAudioTranscriptionResponse::from_json tests ---

    #[test]
    fn from_json_parses_text_and_is_final() {
        let json = r#"{"is_final":true,"text":"hello world","start_time":null,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.text, "hello world");
        assert_eq!(result.transcript, "hello world");
        assert!(result.is_final);
    }

    #[test]
    fn from_json_maps_timing_fields() {
        let json = r#"{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.text, "partial");
        assert!(!result.is_final);
        assert_eq!(result.start_time, Some(1.5));
        assert_eq!(result.end_time, Some(3.0));
    }

    #[test]
    fn from_json_empty_text_parses_successfully() {
        let json = r#"{"is_final":true,"text":"","start_time":null,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.text, "");
        assert!(result.is_final);
    }

    #[test]
    fn from_json_only_start_time_sets_start_time() {
        let json = r#"{"is_final":true,"text":"word","start_time":2.0,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.start_time, Some(2.0));
        assert_eq!(result.end_time, None);
        assert_eq!(result.text, "word");
    }

    #[test]
    fn from_json_invalid_json_returns_error() {
        let result = LiveAudioTranscriptionResponse::from_json("not valid json");
        assert!(result.is_err());
    }

    #[test]
    fn from_json_content_has_text_and_transcript() {
        let json = r#"{"is_final":true,"text":"test","start_time":null,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.text, "test");
        assert_eq!(result.transcript, "test");
    }

    // --- LiveAudioTranscriptionOptions tests ---

    #[test]
    fn options_default_values() {
        let options = LiveAudioTranscriptionOptions::default();

        assert_eq!(options.sample_rate, 16000);
        assert_eq!(options.channels, 1);
        assert_eq!(options.bits_per_sample, 16);
        assert_eq!(options.language, None);
        assert_eq!(options.push_queue_capacity, 100);
    }

    // --- CoreErrorResponse tests ---

    #[test]
    fn core_error_response_try_parse_valid_json() {
        let json =
            r#"{"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}"#;
        let error = CoreErrorResponse::try_parse(json).unwrap();

        assert_eq!(error.code, "ASR_SESSION_NOT_FOUND");
        assert_eq!(error.message, "Session not found");
        assert!(!error.is_transient);
    }

    #[test]
    fn core_error_response_try_parse_invalid_json_returns_none() {
        let result = CoreErrorResponse::try_parse("not json");
        assert!(result.is_none());
    }

    #[test]
    fn core_error_response_try_parse_transient_error() {
        let json = r#"{"code":"BUSY","message":"Model busy","isTransient":true}"#;
        let error = CoreErrorResponse::try_parse(json).unwrap();

        assert!(error.is_transient);
    }
}
