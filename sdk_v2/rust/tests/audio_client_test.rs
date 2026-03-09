//! Integration tests for the [`AudioClient`] transcription API (non-streaming
//! and streaming, with optional temperature).
//!
//! Mirrors `audioClient.test.ts` from the JavaScript SDK.

mod common;

use foundry_local_sdk::openai::AudioClient;
use tokio_stream::StreamExt;

mod tests {
    use super::*;

    // ── Helpers ──────────────────────────────────────────────────────────

    /// Load the whisper model and return an [`AudioClient`] ready for use.
    async fn setup_audio_client() -> AudioClient {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::WHISPER_MODEL_ALIAS)
            .await
            .expect("get_model(whisper-tiny) failed");
        model.load().await.expect("model.load() failed");
        model.create_audio_client()
    }

    fn audio_file() -> String {
        common::get_audio_file_path().to_string_lossy().into_owned()
    }

    // ── Non-streaming transcription ──────────────────────────────────────

    #[tokio::test]
    async fn should_transcribe_audio_without_streaming() {
        let client = setup_audio_client().await;
        let response = client
            .transcribe(&audio_file())
            .await
            .expect("transcribe failed");

        assert!(
            response.text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Transcription should contain expected text, got: {}",
            response.text
        );
    }

    #[tokio::test]
    async fn should_transcribe_audio_without_streaming_with_temperature() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);

        let response = client
            .transcribe(&audio_file())
            .await
            .expect("transcribe with temperature failed");

        assert!(
            response.text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Transcription should contain expected text, got: {}",
            response.text
        );
    }

    // ── Streaming transcription ──────────────────────────────────────────

    #[tokio::test]
    async fn should_transcribe_audio_with_streaming() {
        let client = setup_audio_client().await;
        let mut full_text = String::new();

        let mut stream = client
            .transcribe_streaming(&audio_file())
            .await
            .expect("transcribe_streaming setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            full_text.push_str(&chunk.text);
        }
        stream.close().await.expect("stream close failed");

        assert!(
            full_text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Streamed transcription should contain expected text, got: {full_text}"
        );
    }

    #[tokio::test]
    async fn should_transcribe_audio_with_streaming_with_temperature() {
        let mut client = setup_audio_client().await;
        client.language("en").temperature(0.0);

        let mut full_text = String::new();

        let mut stream = client
            .transcribe_streaming(&audio_file())
            .await
            .expect("transcribe_streaming with temperature setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            full_text.push_str(&chunk.text);
        }
        stream.close().await.expect("stream close failed");

        assert!(
            full_text.contains(common::EXPECTED_TRANSCRIPTION_TEXT),
            "Streamed transcription should contain expected text, got: {full_text}"
        );
    }

    // ── Validation: empty file path ──────────────────────────────────────

    #[tokio::test]
    async fn should_throw_when_transcribing_with_empty_audio_file_path() {
        let client = setup_audio_client().await;
        let result = client.transcribe("").await;
        assert!(result.is_err(), "Expected error for empty audio file path");
    }

    #[tokio::test]
    async fn should_throw_when_transcribing_streaming_with_empty_audio_file_path() {
        let client = setup_audio_client().await;
        let result = client.transcribe_streaming("").await;
        assert!(
            result.is_err(),
            "Expected error for empty audio file path in streaming"
        );
    }
}
