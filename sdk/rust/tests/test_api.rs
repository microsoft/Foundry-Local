mod mock_service;

use foundry_local::{models::ExecutionProvider, FoundryLocalManager};
use mock_service::start_mock_server;
use std::env;

// Helper trait to inject mock service URI into FoundryLocalManager for testing
trait TestableFoundryLocalManager {
    async fn with_test_uri(uri: &str) -> Self;
}

impl TestableFoundryLocalManager for FoundryLocalManager {
    async fn with_test_uri(uri: &str) -> Self {
        let mut manager = FoundryLocalManager::builder()
            .build()
            .await
            .unwrap();
        manager.set_test_service_uri(uri);
        manager
    }
}

#[tokio::test]
async fn test_list_catalog_models() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test listing catalog models
    let catalog_models = manager.list_catalog_models().await.unwrap();

    // Verify the result
    assert_eq!(catalog_models.len(), 2);
    assert_eq!(catalog_models[0].id, "mock-model-1");
    assert_eq!(catalog_models[0].alias, "mock-small");
    assert_eq!(catalog_models[0].runtime, ExecutionProvider::CPU);
    assert_eq!(catalog_models[1].id, "mock-model-2");
    assert_eq!(catalog_models[1].alias, "mock-medium");
    assert_eq!(catalog_models[1].runtime, ExecutionProvider::CUDA);

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_get_model_info() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test getting model info by ID
    let model_info = manager.get_model_info("mock-model-1", false).await.unwrap();
    assert_eq!(model_info.id, "mock-model-1");
    assert_eq!(model_info.alias, "mock-small");

    // Test getting model info by alias
    let model_info = manager.get_model_info("mock-small", false).await.unwrap();
    assert_eq!(model_info.id, "mock-model-1");
    assert_eq!(model_info.alias, "mock-small");

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_get_cache_location() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test getting cache location
    let cache_location = manager.get_cache_location().await.unwrap();
    assert_eq!(cache_location, "/tmp/mock-cache");

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_list_cached_models() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test listing cached models
    let cached_models = manager.list_cached_models().await.unwrap();
    assert_eq!(cached_models.len(), 1);
    assert_eq!(cached_models[0].id, "mock-model-1");

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_download_model() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test downloading a model
    let model_info = manager
        .download_model("mock-model-2", None, false)
        .await
        .unwrap();
    assert_eq!(model_info.id, "mock-model-2");

    // Verify the model is now cached
    let cached_models = manager.list_cached_models().await.unwrap();
    assert_eq!(cached_models.len(), 2);
    assert!(cached_models.iter().any(|m| m.id == "mock-model-1"));
    assert!(cached_models.iter().any(|m| m.id == "mock-model-2"));

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_load_and_unload_model() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test loading a model
    let model_info = manager.load_model("mock-model-1", Some(300)).await.unwrap();
    assert_eq!(model_info.id, "mock-model-1");

    // Verify the model is loaded
    let loaded_models = manager.list_loaded_models().await.unwrap();
    assert_eq!(loaded_models.len(), 1);
    assert_eq!(loaded_models[0].id, "mock-model-1");

    // Test unloading the model
    manager.unload_model("mock-model-1", false).await.unwrap();

    // Verify the model is unloaded
    let loaded_models = manager.list_loaded_models().await.unwrap();
    assert_eq!(loaded_models.len(), 0);

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_endpoint_and_api_key() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test getting endpoint
    let endpoint = manager.endpoint().unwrap();
    assert_eq!(endpoint, format!("{}/v1", server_uri));

    // Test getting API key (default)
    let api_key = manager.api_key();
    assert_eq!(api_key, "OPENAI_API_KEY");

    // Test getting API key (with env var)
    env::set_var("OPENAI_API_KEY", "test-api-key");
    let api_key = manager.api_key();
    assert_eq!(api_key, "test-api-key");

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}
