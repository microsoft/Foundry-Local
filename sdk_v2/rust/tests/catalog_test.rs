//! Integration tests for the [`Catalog`] API.
//!
//! Mirrors `catalog.test.ts` from the JavaScript SDK.

mod common;

use foundry_local_sdk::Catalog;

mod tests {
    use super::*;

    // ── Helpers ──────────────────────────────────────────────────────────

    fn catalog() -> &'static Catalog {
        common::get_test_manager().catalog()
    }

    // ── Basic catalogue access ───────────────────────────────────────────

    /// The catalog should expose a non-empty name after initialisation.
    #[test]
    #[ignore = "requires native Foundry Local library"]
    fn should_initialize_with_catalog_name() {
        let cat = catalog();
        let name = cat.name();
        assert!(!name.is_empty(), "Catalog name must not be empty");
    }

    /// `list_models()` should return at least one model and the test model
    /// should be present among them.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_list_models() {
        let cat = catalog();
        let models = cat.get_models().await.expect("get_models failed");

        assert!(!models.is_empty(), "Expected at least one model in the catalog");

        let found = models.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            found,
            "Test model '{}' not found in catalog",
            common::TEST_MODEL_ALIAS
        );
    }

    /// `get_model()` with a valid alias should return the corresponding model.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_get_model_by_alias() {
        let cat = catalog();
        let model = cat
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");

        assert_eq!(model.alias(), common::TEST_MODEL_ALIAS);
    }

    // ── Validation: empty / invalid alias ────────────────────────────────

    /// `get_model("")` should return an error containing
    /// "Model alias must be a non-empty string".
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_throw_when_getting_model_with_empty_alias() {
        let cat = catalog();
        let result = cat.get_model("").await;
        assert!(result.is_err(), "Expected error for empty alias");

        let err_msg = result.unwrap_err().to_string();
        assert!(
            err_msg.contains("Model alias must be a non-empty string"),
            "Unexpected error message: {err_msg}"
        );
    }

    /// An unknown alias should produce an error mentioning "not found" and
    /// listing available models.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_throw_when_getting_model_with_unknown_alias() {
        let cat = catalog();
        let result = cat.get_model("unknown-nonexistent-model-alias").await;
        assert!(result.is_err(), "Expected error for unknown alias");

        let err_msg = result.unwrap_err().to_string();
        assert!(
            err_msg.contains("not found"),
            "Error should mention 'not found': {err_msg}"
        );
        assert!(
            err_msg.contains("Available models"),
            "Error should list available models: {err_msg}"
        );
    }

    // ── Cached models ────────────────────────────────────────────────────

    /// `get_cached_models()` should return at least one model and the test
    /// model should be cached.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_get_cached_models() {
        let cat = catalog();
        let cached = cat.get_cached_models().await.expect("get_cached_models failed");

        assert!(!cached.is_empty(), "Expected at least one cached model");

        let found = cached.iter().any(|m| m.alias() == common::TEST_MODEL_ALIAS);
        assert!(
            found,
            "Test model '{}' should be in the cached models list",
            common::TEST_MODEL_ALIAS
        );
    }

    // ── Model variant validation ─────────────────────────────────────────

    /// `get_model_variant("")` should return a validation error.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_throw_when_getting_model_variant_with_empty_id() {
        let cat = catalog();
        let result = cat.get_model_variant("").await;
        assert!(result.is_err(), "Expected error for empty variant ID");
    }

    /// `get_model_variant()` with an unknown ID should return an error.
    #[tokio::test]
    #[ignore = "requires native Foundry Local library"]
    async fn should_throw_when_getting_model_variant_with_unknown_id() {
        let cat = catalog();
        let result = cat.get_model_variant("unknown-nonexistent-variant-id").await;
        assert!(result.is_err(), "Expected error for unknown variant ID");
    }
}
