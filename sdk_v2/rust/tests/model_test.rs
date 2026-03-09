//! Integration tests for the [`Model`] lifecycle (cache verification,
//! load / unload).
//!
//! Mirrors `model.test.ts` from the JavaScript SDK.

mod common;


mod tests {
    use super::*;

    // ── Cache verification ───────────────────────────────────────────────

    /// The shared test-data directory should contain pre-cached models for
    /// both `qwen2.5-0.5b` and `whisper-tiny`.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_verify_cached_models_from_test_data_shared() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let cached = catalog.get_cached_models().await.expect("get_cached_models failed");

        // qwen2.5-0.5b must be cached
        let has_qwen = cached.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            has_qwen,
            "'{}' should be present in cached models",
            common::TEST_MODEL_ALIAS
        );

        // whisper-tiny must be cached
        let has_whisper = cached.iter().any(|m| m.alias() == common::WHISPER_MODEL_ALIAS);
        assert!(
            has_whisper,
            "'{}' should be present in cached models",
            common::WHISPER_MODEL_ALIAS
        );
    }

    // ── Load / unload lifecycle ──────────────────────────────────────────

    /// Loading a model should mark it as loaded; unloading should mark it as
    /// not loaded.
    ///
    /// Timeout note: the JS test uses a 120 s timeout for this test.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_load_and_unload_model() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");

        // Load
        model.load().await.expect("model.load() failed");
        assert!(
            model.is_loaded().await.expect("is_loaded check failed"),
            "Model should be loaded after load()"
        );

        // Unload
        model.unload().await.expect("model.unload() failed");
        assert!(
            !model.is_loaded().await.expect("is_loaded check failed"),
            "Model should not be loaded after unload()"
        );
    }
}
