//! Integration tests for model loading and unloading through the public API.
//!
//! Mirrors `modelLoadManager.test.ts` from the JavaScript SDK.
//!
//! **Note:** In the JavaScript SDK these tests access the private
//! `coreInterop` property via an `as any` cast.  In Rust, `CoreInterop` and
//! `ModelLoadManager::new` are `pub(crate)` and cannot be reached from
//! integration tests.  Instead, we exercise model loading and unloading
//! through the public [`Model`] and [`Catalog`] APIs which internally
//! delegate to `ModelLoadManager`.

mod common;

mod tests {
    use super::*;

    // ── Helpers ──────────────────────────────────────────────────────────

    /// Return the test model from the catalog.
    async fn get_test_model() -> foundry_local_sdk::Model {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed")
    }

    // ── Core-interop path ────────────────────────────────────────────────

    /// Loading a model via the core interop (in-process) path should succeed.
    ///
    /// Timeout note: the JS test uses a 120 s timeout.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_load_model_using_core_interop() {
        let model = get_test_model().await;

        model
            .load()
            .await
            .expect("model.load() failed");
    }

    /// Unloading a previously loaded model via the core interop path should
    /// succeed.
    ///
    /// Timeout note: the JS test uses a 120 s timeout.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_unload_model_using_core_interop() {
        let model = get_test_model().await;

        // Ensure the model is loaded first.
        model
            .load()
            .await
            .expect("model.load() failed");

        model
            .unload()
            .await
            .expect("model.unload() failed");
    }

    /// Listing loaded models via the core interop path should return a
    /// collection (possibly empty, but the call itself must succeed).
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_list_loaded_models_using_core_interop() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();

        let loaded = catalog
            .get_loaded_models()
            .await
            .expect("catalog.get_loaded_models() failed");

        // The result should be a valid (possibly empty) list of model IDs.
        // (Vec<String> is always valid; just ensure the call succeeded.)
        let _ = loaded;
    }

    // ── External web-service path ────────────────────────────────────────

    /// Loading and unloading a model through the external HTTP service should
    /// succeed.
    ///
    /// This test is skipped in CI because it requires a running web service.
    ///
    /// Timeout note: the JS test uses a 120 s timeout.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library and running web service"]
    async fn should_load_and_unload_model_using_external_service() {
        if common::is_running_in_ci() {
            eprintln!("Skipping external-service test in CI");
            return;
        }

        let manager = common::get_test_manager();
        let model = get_test_model().await;

        // Start the web service so we can test the HTTP path.
        let _urls = manager
            .start_web_service()
            .await
            .expect("start_web_service failed");

        // Load via the model API (delegates to ModelLoadManager internally).
        model
            .load()
            .await
            .expect("load via external service failed");

        // Unload
        model
            .unload()
            .await
            .expect("unload via external service failed");
    }

    /// Listing loaded models through the external HTTP service should succeed.
    ///
    /// This test is skipped in CI because it requires a running web service.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library and running web service"]
    async fn should_list_loaded_models_using_external_service() {
        if common::is_running_in_ci() {
            eprintln!("Skipping external-service test in CI");
            return;
        }

        let manager = common::get_test_manager();

        let _urls = manager
            .start_web_service()
            .await
            .expect("start_web_service failed");

        let catalog = manager.catalog();
        let loaded = catalog
            .get_loaded_models()
            .await
            .expect("get_loaded_models via external service failed");

        // Vec<String> is always a valid list; just ensure the call succeeded.
        let _ = loaded;
    }
}
