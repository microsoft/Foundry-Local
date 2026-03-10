//! Shared test utilities and configuration for Foundry Local SDK integration tests.
//!
//! Mirrors `testUtils.ts` from the JavaScript SDK test suite.

#![allow(dead_code)]

use std::collections::HashMap;
use std::path::PathBuf;

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager, LogLevel};

/// Default model alias used for chat-completion integration tests.
pub const TEST_MODEL_ALIAS: &str = "qwen2.5-0.5b";

/// Default model alias used for audio-transcription integration tests.
pub const WHISPER_MODEL_ALIAS: &str = "whisper-tiny";

/// Expected transcription text fragment for the shared audio test file.
pub const EXPECTED_TRANSCRIPTION_TEXT: &str =
    " And lots of times you need to give people more than one link at a time";

// ── Environment helpers ──────────────────────────────────────────────────────

/// Returns `true` when the tests are running inside a CI environment
/// (Azure DevOps or GitHub Actions).
pub fn is_running_in_ci() -> bool {
    let azure_devops = std::env::var("TF_BUILD").unwrap_or_else(|_| "false".into());
    let github_actions = std::env::var("GITHUB_ACTIONS").unwrap_or_else(|_| "false".into());
    azure_devops.eq_ignore_ascii_case("true") || github_actions.eq_ignore_ascii_case("true")
}

/// Walk upward from `CARGO_MANIFEST_DIR` until a `.git` directory is found.
pub fn get_git_repo_root() -> PathBuf {
    let mut current = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    loop {
        if current.join(".git").exists() {
            return current;
        }
        if !current.pop() {
            panic!(
                "Could not locate git repo root starting from {}",
                env!("CARGO_MANIFEST_DIR")
            );
        }
    }
}

/// Path to the shared test-data directory that lives alongside the repo root.
pub fn get_test_data_shared_path() -> PathBuf {
    let repo_root = get_git_repo_root();
    repo_root
        .parent()
        .expect("repo root has no parent")
        .join("test-data-shared")
}

/// Path to the shared audio test file used by audio-client tests.
pub fn get_audio_file_path() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("testdata")
        .join("Recording.mp3")
}

// ── Test configuration ───────────────────────────────────────────────────────

/// Build a [`FoundryLocalConfig`] suitable for integration tests.
///
/// * `modelCacheDir`  → `<repo-root>/../test-data-shared`
/// * `logsDir`        → `<repo-root>/sdk_v2/rust/logs`
/// * `logLevel`       → `Warn`
/// * `Bootstrap`      → `false` (via additional settings)
pub fn test_config() -> FoundryLocalConfig {
    let repo_root = get_git_repo_root();
    let logs_dir = repo_root.join("sdk_v2").join("rust").join("logs");

    let mut additional = HashMap::new();
    additional.insert("Bootstrap".into(), "false".into());

    let mut config = FoundryLocalConfig::new("FoundryLocalTest");
    config.model_cache_dir = Some(get_test_data_shared_path().to_string_lossy().into_owned());
    config.logs_dir = Some(logs_dir.to_string_lossy().into_owned());
    config.log_level = Some(LogLevel::Warn);
    config.additional_settings = Some(additional);
    config
}

/// Create (or return the cached) [`FoundryLocalManager`] for tests.
///
/// Panics if creation fails so that test set-up failures are immediately
/// visible.
pub fn get_test_manager() -> &'static FoundryLocalManager {
    FoundryLocalManager::create(test_config()).expect("Failed to create FoundryLocalManager")
}

// ── Tool definitions ─────────────────────────────────────────────────────────

/// Returns a tool definition for a simple "multiply" function.
///
/// Used by tool-calling chat-completion tests.
pub fn get_multiply_tool() -> foundry_local_sdk::ChatCompletionTools {
    serde_json::from_value(serde_json::json!({
        "type": "function",
        "function": {
            "name": "multiply",
            "description": "Multiply two numbers together",
            "parameters": {
                "type": "object",
                "properties": {
                    "a": {
                        "type": "number",
                        "description": "The first number"
                    },
                    "b": {
                        "type": "number",
                        "description": "The second number"
                    }
                },
                "required": ["a", "b"]
            }
        }
    }))
    .expect("Failed to parse multiply tool definition")
}
