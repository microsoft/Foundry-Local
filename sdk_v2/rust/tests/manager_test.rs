//! Integration tests for [`FoundryLocalManager`] initialisation.
//!
//! Mirrors `foundryLocalManager.test.ts` from the JavaScript SDK.

mod common;

use foundry_local_sdk::FoundryLocalManager;

mod tests {
    use super::*;

    // ── Initialisation ───────────────────────────────────────────────────

    /// The manager should initialise successfully with the test configuration.
    #[test]
    #[ignore = "requires native Foundry Local library"]
    fn should_initialize_successfully() {
        let config = common::test_config();
        let manager = FoundryLocalManager::create(config);
        assert!(
            manager.is_ok(),
            "Manager creation failed: {:?}",
            manager.err()
        );
    }

    /// The catalog obtained from a freshly-created manager should have a
    /// non-empty name.
    #[test]
    #[ignore = "requires native Foundry Local library"]
    fn should_return_catalog_with_non_empty_name() {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let name = catalog.name();
        assert!(!name.is_empty(), "Catalog name should not be empty");
    }
}
