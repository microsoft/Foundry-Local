# Foundry Local SDK Tests

This directory contains tests for the Foundry Local SDK, with a focus on testing the API without requiring an actual Foundry Local service to be running.

## Mock Service

The `mock_service.rs` file contains a mock implementation of the Foundry Local service that can be used for testing. The mock service provides endpoints that mimic the behavior of the real service, but with predictable responses and no actual model loading or execution.

Key features:
- In-memory state management for models, cache, etc.
- HTTP endpoints that match the real service
- Configurable responses

## Running Tests

To run all tests:

```bash
cargo test --package foundry-local
```

To run a specific test:

```bash
cargo test --package foundry-local test_list_catalog_models
```

To run the integration tests:
```bash
cargo test --features integration-tests
```

## Adding New Tests

When adding new tests:

1. Add test functions to `test_api.rs`
2. Make sure each test starts its own mock server and shuts it down when done
3. Use the `TestableFoundryLocalManager` trait to create a manager that uses the mock server

Example:

```rust
#[tokio::test]
async fn test_my_new_feature() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;
    
    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri);
    
    // Test your feature
    // ...
    
    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}
```

## Extending the Mock Service

To add new endpoints or behaviors to the mock service:

1. Add new state fields to `MockState` if needed
2. Implement handler functions for the new endpoints
3. Add routes to the router in `start_mock_server` 