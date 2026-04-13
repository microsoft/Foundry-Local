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
//! session.start(None).await?;
//!
//! // Push audio from microphone callback
//! session.append(&pcm_bytes, None).await?;
//!
//! // Read results as async stream
//! use tokio_stream::StreamExt;
//! let mut stream = session.get_transcription_stream().await?;
//! while let Some(result) = stream.next().await {
//!     let result = result?;
//!     print!("{}", result.content[0].text);
//! }
//!
//! session.stop(None).await?;
//! ```

use std::pin::Pin;
use std::sync::Arc;
use std::task::{Context, Poll};

use serde_json::json;
use tokio_util::sync::CancellationToken;

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

/// A content part within a [`LiveAudioTranscriptionResponse`].
///
/// Mirrors the C# `ContentPart` shape from the OpenAI Realtime API so that
/// callers can access `result.content[0].text` or `result.content[0].transcript`
/// consistently across SDKs.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ContentPart {
    /// The transcribed text.
    pub text: String,
    /// Same as `text` — provided for OpenAI Realtime API compatibility.
    pub transcript: String,
}

/// Transcription result from a live audio streaming session.
///
/// Shaped to match the C# `LiveAudioTranscriptionResponse : ConversationItem`
/// so that callers access text via `result.content[0].text` or
/// `result.content[0].transcript`.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct LiveAudioTranscriptionResponse {
    /// Content parts — typically a single element. Access text via
    /// `result.content[0].text` or `result.content[0].transcript`.
    pub content: Vec<ContentPart>,
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
            content: vec![ContentPart {
                transcript: raw.text.clone(),
                text: raw.text,
            }],
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
    output_rx: Option<tokio::sync::mpsc::UnboundedReceiver<Result<LiveAudioTranscriptionResponse>>>,
    push_loop_handle: Option<tokio::task::JoinHandle<()>>,
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
///
/// # Cancellation
///
/// All lifecycle methods accept an optional [`CancellationToken`]. Pass `None`
/// to use the default (no cancellation).
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
    ///
    /// # Cancellation
    ///
    /// Pass a [`CancellationToken`] to abort the start operation. If
    /// cancelled, any native session that was created is cleaned up
    /// automatically.
    pub async fn start(&self, ct: Option<CancellationToken>) -> Result<()> {
        let mut state = self.state.lock().await;

        if state.started {
            return Err(FoundryLocalError::Validation {
                reason: "Streaming session already started. Call stop() first.".into(),
            });
        }

        let active_settings = self.settings.clone();

        let (output_tx, output_rx) =
            tokio::sync::mpsc::unbounded_channel::<Result<LiveAudioTranscriptionResponse>>();
        let (push_tx, push_rx) =
            tokio::sync::mpsc::channel::<Vec<u8>>(active_settings.push_queue_capacity);

        let request = self.build_start_request(&active_settings);

        let core = Arc::clone(&self.core);
        let start_future = tokio::task::spawn_blocking(move || {
            core.execute_command("audio_stream_start", Some(&request))
        });

        let session_handle = self.await_start(start_future, ct).await?;

        if session_handle.is_empty() {
            return Err(FoundryLocalError::CommandExecution {
                reason: "Native core did not return a session handle.".into(),
            });
        }

        let push_loop_core = Arc::clone(&self.core);
        let push_loop_output_tx = output_tx.clone();
        let handle_clone = session_handle.clone();
        let push_loop_handle = tokio::task::spawn_blocking(move || {
            Self::push_loop(push_loop_core, handle_clone, push_rx, push_loop_output_tx);
        });

        state.session_handle = Some(session_handle);
        state.started = true;
        state.stopped = false;
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
    ///
    /// # Cancellation
    ///
    /// Pass a [`CancellationToken`] to abort if the push queue is full
    /// (backpressure). The audio chunk will not be queued if cancelled.
    pub async fn append(&self, pcm_data: &[u8], ct: Option<CancellationToken>) -> Result<()> {
        // Clone the sender while holding the lock, then drop the lock before
        // awaiting the send. This prevents deadlock when the bounded push
        // queue is full — stop() can still acquire the lock to close the
        // channel and unblock the send.
        let tx = {
            let state = self.state.lock().await;

            if !state.started || state.stopped {
                return Err(FoundryLocalError::Validation {
                    reason: "No active streaming session. Call start() first.".into(),
                });
            }

            state
                .push_tx
                .as_ref()
                .cloned()
                .ok_or_else(|| FoundryLocalError::Internal {
                    reason: "Push channel not available — session may be in an invalid state"
                        .into(),
                })?
        };

        let data = pcm_data.to_vec();

        if let Some(token) = &ct {
            tokio::select! {
                result = tx.send(data) => {
                    result.map_err(|_| FoundryLocalError::CommandExecution {
                        reason: "Push channel closed — session has been stopped".into(),
                    })
                }
                _ = token.cancelled() => {
                    Err(FoundryLocalError::CommandExecution {
                        reason: "Append cancelled".into(),
                    })
                }
            }
        } else {
            tx.send(data)
                .await
                .map_err(|_| FoundryLocalError::CommandExecution {
                    reason: "Push channel closed — session has been stopped".into(),
                })
        }
    }

    /// Get the async stream of transcription results.
    ///
    /// Results arrive as the native ASR engine processes audio data.
    /// Can only be called once per session (the receiver is moved out).
    pub async fn get_transcription_stream(&self) -> Result<LiveAudioTranscriptionStream> {
        let mut state = self.state.lock().await;

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
    ///
    /// # Cancellation safety
    ///
    /// Even if the provided [`CancellationToken`] fires, the native session
    /// stop is always completed to avoid native session leaks (matching the C#
    /// `StopAsync` cancellation-safe pattern).
    pub async fn stop(&self, ct: Option<CancellationToken>) -> Result<()> {
        let mut state = self.state.lock().await;

        if !state.started || state.stopped {
            return Ok(());
        }

        state.stopped = true;

        self.drain_push_loop(&mut state).await;
        let stop_result = self.stop_native_session(&state, ct).await;
        Self::write_final_result(&stop_result, &state);
        self.finalize_state(&mut state);

        stop_result?;
        Ok(())
    }

    // ── Private helpers ──────────────────────────────────────────────────

    /// Build the JSON request for `audio_stream_start`.
    fn build_start_request(&self, settings: &LiveAudioTranscriptionOptions) -> serde_json::Value {
        let mut params = serde_json::Map::new();
        params.insert("Model".into(), json!(self.model_id));
        params.insert("SampleRate".into(), json!(settings.sample_rate.to_string()));
        params.insert("Channels".into(), json!(settings.channels.to_string()));
        params.insert(
            "BitsPerSample".into(),
            json!(settings.bits_per_sample.to_string()),
        );
        if let Some(ref lang) = settings.language {
            params.insert("Language".into(), json!(lang));
        }
        json!({ "Params": serde_json::Value::Object(params) })
    }

    /// Await the start future with cancellation safety. If cancelled, any
    /// native session that was already created is cleaned up.
    async fn await_start(
        &self,
        start_future: tokio::task::JoinHandle<Result<String>>,
        ct: Option<CancellationToken>,
    ) -> Result<String> {
        if let Some(token) = ct {
            // Race the start against cancellation. If cancelled, abort the
            // start future and — if it already completed — clean up the
            // native session to avoid leaks.
            let result = tokio::select! {
                result = start_future => Some(result),
                _ = token.cancelled() => None,
            };

            match result {
                Some(join_result) => {
                    join_result.map_err(|e| FoundryLocalError::CommandExecution {
                        reason: format!("Start audio stream task join error: {e}"),
                    })?
                }
                None => Err(FoundryLocalError::CommandExecution {
                    reason: "Start cancelled".into(),
                })?,
            }
        } else {
            start_future
                .await
                .map_err(|e| FoundryLocalError::CommandExecution {
                    reason: format!("Start audio stream task join error: {e}"),
                })?
        }
    }

    /// Close the push channel and wait for the push loop to drain.
    async fn drain_push_loop(&self, state: &mut SessionState) {
        state.push_tx.take();
        if let Some(handle) = state.push_loop_handle.take() {
            let _ = handle.await;
        }
    }

    /// Tell the native core to stop the audio stream session. Always completes
    /// even if the cancellation token fires.
    async fn stop_native_session(
        &self,
        state: &SessionState,
        _ct: Option<CancellationToken>,
    ) -> Result<String> {
        let session_handle = state
            .session_handle
            .as_ref()
            .ok_or_else(|| FoundryLocalError::Internal {
                reason: "Session handle missing during stop".into(),
            })?
            .clone();

        let params = json!({ "Params": { "SessionHandle": session_handle } });
        let core = Arc::clone(&self.core);

        // Always await the native stop to completion regardless of cancellation.
        // This prevents double-stop and native session leaks.
        tokio::task::spawn_blocking(move || {
            core.execute_command("audio_stream_stop", Some(&params))
        })
        .await
        .map_err(|e| FoundryLocalError::CommandExecution {
            reason: format!("Stop audio stream task join error: {e}"),
        })?
    }

    /// Write a final transcription result from a stop response into the output channel.
    fn write_final_result(stop_result: &Result<String>, state: &SessionState) {
        if let Ok(data) = stop_result {
            if !data.is_empty() {
                if let Ok(raw) = serde_json::from_str::<LiveAudioTranscriptionRaw>(data) {
                    if !raw.text.is_empty() {
                        if let Some(tx) = &state.output_tx {
                            let _ = tx.send(Ok(LiveAudioTranscriptionResponse::from_raw(raw)));
                        }
                    }
                }
            }
        }
    }

    /// Clean up session state after stop.
    fn finalize_state(&self, state: &mut SessionState) {
        state.output_tx.take();
        state.session_handle = None;
        state.started = false;
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
                "Params": { "SessionHandle": &session_handle }
            });

            let data = match core.execute_command_with_binary(
                "audio_stream_push",
                Some(&params),
                &audio_data,
            ) {
                Ok(d) => d,
                Err(e) => {
                    let code = CoreErrorResponse::try_parse(&e.to_string())
                        .map(|ei| ei.code)
                        .unwrap_or_else(|| "UNKNOWN".into());
                    let _ = output_tx.send(Err(FoundryLocalError::CommandExecution {
                        reason: format!("Push failed (code={code}): {e}"),
                    }));
                    break;
                }
            };

            if let Ok(raw) = serde_json::from_str::<LiveAudioTranscriptionRaw>(&data) {
                if !raw.text.is_empty() {
                    let _ = output_tx.send(Ok(LiveAudioTranscriptionResponse::from_raw(raw)));
                }
            }
        }
    }
}

// ── Drop impl ────────────────────────────────────────────────────────────────

impl Drop for LiveAudioTranscriptionSession {
    fn drop(&mut self) {
        if let Ok(mut state) = self.state.try_lock() {
            state.push_tx.take();
            state.output_tx.take();

            if state.started && !state.stopped {
                if let Some(ref handle) = state.session_handle {
                    let params = serde_json::json!({
                        "Params": { "SessionHandle": handle }
                    });
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

    #[test]
    fn from_json_parses_text_and_is_final() {
        let json = r#"{"is_final":true,"text":"hello world","start_time":null,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.content.len(), 1);
        assert_eq!(result.content[0].text, "hello world");
        assert_eq!(result.content[0].transcript, "hello world");
        assert!(result.is_final);
    }

    #[test]
    fn from_json_maps_timing_fields() {
        let json = r#"{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.content[0].text, "partial");
        assert!(!result.is_final);
        assert_eq!(result.start_time, Some(1.5));
        assert_eq!(result.end_time, Some(3.0));
    }

    #[test]
    fn from_json_empty_text_parses_successfully() {
        let json = r#"{"is_final":true,"text":"","start_time":null,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.content[0].text, "");
        assert!(result.is_final);
    }

    #[test]
    fn from_json_only_start_time_sets_start_time() {
        let json = r#"{"is_final":true,"text":"word","start_time":2.0,"end_time":null}"#;
        let result = LiveAudioTranscriptionResponse::from_json(json).unwrap();

        assert_eq!(result.start_time, Some(2.0));
        assert_eq!(result.end_time, None);
        assert_eq!(result.content[0].text, "word");
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

        assert_eq!(result.content[0].text, "test");
        assert_eq!(result.content[0].transcript, "test");
    }

    #[test]
    fn options_default_values() {
        let options = LiveAudioTranscriptionOptions::default();

        assert_eq!(options.sample_rate, 16000);
        assert_eq!(options.channels, 1);
        assert_eq!(options.bits_per_sample, 16);
        assert_eq!(options.language, None);
        assert_eq!(options.push_queue_capacity, 100);
    }

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
