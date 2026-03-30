//! OpenAI-compatible audio transcription client.

use std::path::Path;
use std::sync::Arc;

use serde_json::{json, Value};

use crate::detail::core_interop::CoreInterop;
use crate::error::{FoundryLocalError, Result};

use super::json_stream::JsonStream;

/// A segment of a transcription, as returned by the OpenAI-compatible API.
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct TranscriptionSegment {
    /// Segment index.
    pub id: i32,
    /// Seek offset of the segment.
    pub seek: i32,
    /// Start time of the segment in seconds.
    pub start: f64,
    /// End time of the segment in seconds.
    pub end: f64,
    /// Transcribed text of the segment.
    pub text: String,
    /// Token IDs corresponding to the text.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tokens: Option<Vec<i32>>,
    /// Temperature used for generation.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub temperature: Option<f64>,
    /// Average log probability of the segment.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub avg_logprob: Option<f64>,
    /// Compression ratio of the segment.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub compression_ratio: Option<f64>,
    /// Probability of no speech in the segment.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub no_speech_prob: Option<f64>,
}

/// A word with timing information, as returned by the OpenAI-compatible API.
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct TranscriptionWord {
    /// The word text.
    pub word: String,
    /// Start time of the word in seconds.
    pub start: f64,
    /// End time of the word in seconds.
    pub end: f64,
}

/// OpenAI-compatible audio transcription response.
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct AudioTranscriptionResponse {
    /// The transcribed text.
    pub text: String,
    /// The language of the input audio (if detected).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub language: Option<String>,
    /// Duration of the input audio in seconds (if available).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub duration: Option<f64>,
    /// Segments of the transcription (if available).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub segments: Option<Vec<TranscriptionSegment>>,
    /// Words with timestamps (if available).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub words: Option<Vec<TranscriptionWord>>,
}

/// Tuning knobs for audio transcription requests.
///
/// Use the chainable setter methods to configure, e.g.:
///
/// ```ignore
/// let client = model.create_audio_client()
///     .language("en")
///     .temperature(0.2);
/// ```
#[derive(Debug, Clone, Default)]
pub struct AudioClientSettings {
    language: Option<String>,
    temperature: Option<f64>,
}

impl AudioClientSettings {
    fn serialize(&self, model_id: &str, file_name: &str) -> Value {
        let mut map = serde_json::Map::new();

        map.insert("Model".into(), json!(model_id));
        map.insert("FileName".into(), json!(file_name));

        if let Some(ref lang) = self.language {
            map.insert("Language".into(), json!(lang));
        }
        if let Some(temp) = self.temperature {
            map.insert("Temperature".into(), json!(temp));
        }

        Value::Object(map)
    }
}

/// A stream of [`AudioTranscriptionResponse`] chunks.
///
/// Returned by [`AudioClient::transcribe_streaming`].
pub type AudioTranscriptionStream = JsonStream<AudioTranscriptionResponse>;

/// Client for OpenAI-compatible audio transcription backed by a local model.
pub struct AudioClient {
    model_id: String,
    core: Arc<CoreInterop>,
    settings: AudioClientSettings,
}

impl AudioClient {
    pub(crate) fn new(model_id: &str, core: Arc<CoreInterop>) -> Self {
        Self {
            model_id: model_id.to_owned(),
            core,
            settings: AudioClientSettings::default(),
        }
    }

    /// Set the language hint for transcription.
    pub fn language(mut self, lang: impl Into<String>) -> Self {
        self.settings.language = Some(lang.into());
        self
    }

    /// Set the sampling temperature.
    pub fn temperature(mut self, v: f64) -> Self {
        self.settings.temperature = Some(v);
        self
    }

    /// Transcribe an audio file.
    pub async fn transcribe(
        &self,
        audio_file_path: impl AsRef<Path>,
    ) -> Result<AudioTranscriptionResponse> {
        let path_str =
            audio_file_path
                .as_ref()
                .to_str()
                .ok_or_else(|| FoundryLocalError::Validation {
                    reason: "audio file path is not valid UTF-8".into(),
                })?;
        Self::validate_path(path_str)?;

        let request = self.settings.serialize(&self.model_id, path_str);
        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let raw = self
            .core
            .execute_command_async("audio_transcribe".into(), Some(params))
            .await?;
        let parsed: AudioTranscriptionResponse = serde_json::from_str(&raw)?;
        Ok(parsed)
    }

    /// Transcribe an audio file with streaming results, returning an
    /// [`AudioTranscriptionStream`].
    pub async fn transcribe_streaming(
        &self,
        audio_file_path: impl AsRef<Path>,
    ) -> Result<AudioTranscriptionStream> {
        let path_str =
            audio_file_path
                .as_ref()
                .to_str()
                .ok_or_else(|| FoundryLocalError::Validation {
                    reason: "audio file path is not valid UTF-8".into(),
                })?;
        Self::validate_path(path_str)?;

        let request = self.settings.serialize(&self.model_id, path_str);

        let params = json!({
            "Params": {
                "OpenAICreateRequest": serde_json::to_string(&request)?
            }
        });

        let rx = self
            .core
            .execute_command_streaming_channel("audio_transcribe".into(), Some(params))
            .await?;

        Ok(AudioTranscriptionStream::new(rx))
    }

    fn validate_path(path: &str) -> Result<()> {
        if path.trim().is_empty() {
            return Err(FoundryLocalError::Validation {
                reason: "audio_file_path must be a non-empty string".into(),
            });
        }
        Ok(())
    }
}
