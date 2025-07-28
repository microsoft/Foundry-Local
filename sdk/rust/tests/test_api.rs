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
        let mut manager = FoundryLocalManager::builder().build().await.unwrap();
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
    assert_eq!(catalog_models.len(), 3);
    assert_eq!(catalog_models[0].id, "Phi-4-mini-instruct-generic-cpu:1");
    assert_eq!(catalog_models[0].alias, "phi-4-mini");
    assert_eq!(catalog_models[0].runtime, ExecutionProvider::CPU);
    assert_eq!(catalog_models[1].id, "qwen2.5-0.5b-instruct-cuda-gpu:1");
    assert_eq!(catalog_models[1].alias, "qwen2.5-0.5b");
    assert_eq!(catalog_models[1].runtime, ExecutionProvider::CUDA);
    assert_eq!(catalog_models[2].id, "qwen2.5-0.5b-instruct-cuda-gpu:2");
    assert_eq!(catalog_models[2].alias, "qwen2.5-0.5b");
    assert_eq!(catalog_models[2].runtime, ExecutionProvider::CUDA);

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
    let model_info = manager.get_model_info("Phi-4-mini-instruct-generic-cpu:1", false).await.unwrap();
    assert_eq!(model_info.id, "Phi-4-mini-instruct-generic-cpu:1");
    assert_eq!(model_info.alias, "phi-4-mini");

    // Test getting model info by alias
    let model_info = manager.get_model_info("qwen2.5-0.5b", false).await.unwrap();
    assert_eq!(model_info.id, "qwen2.5-0.5b-instruct-cuda-gpu:2");
    assert_eq!(model_info.alias, "qwen2.5-0.5b");

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
    assert_eq!(cached_models[0].id, "qwen2.5-0.5b-instruct-cuda-gpu:1");

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
        .download_model("qwen2.5-0.5b", None, false)
        .await
        .unwrap();
    assert_eq!(model_info.id, "qwen2.5-0.5b-instruct-cuda-gpu:2");

    // Verify latest version of the model is now also cached
    let cached_models = manager.list_cached_models().await.unwrap();
    assert_eq!(cached_models.len(), 2);
    assert!(cached_models.iter().any(|m| m.id == "qwen2.5-0.5b-instruct-cuda-gpu:1"));
    assert!(cached_models.iter().any(|m| m.id == "qwen2.5-0.5b-instruct-cuda-gpu:2"));

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_is_model_upgradeable() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // When no version is in the cache
    let is_upgradeable = manager.is_model_upgradeable("phi-4-mini").await.unwrap();
    assert!(is_upgradeable, "Expected upgradeable because latest version is not cached");

    // When the latest version is not in the cache
    let is_upgradeable = manager.is_model_upgradeable("qwen2.5-0.5b").await.unwrap();
    assert!(is_upgradeable, "Expected upgradeable because latest version is not cached");

    // Shutdown the mock server
    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_upgrade_model_success() {
    let (server_uri, shutdown_tx) = start_mock_server().await;
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // This should trigger download of latest model "qwen2.5-0.5b-instruct-cuda-gpu:2"
    let model_info = manager.upgrade_model("qwen2.5-0.5b", None).await.unwrap();

    // Assert returned model info is correct (example)
    assert_eq!(model_info.id, "qwen2.5-0.5b-instruct-cuda-gpu:2");

    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_upgrade_model_not_found() {
    let (server_uri, shutdown_tx) = start_mock_server().await;
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Call upgrade_model without unwrap to get Result
    let result = manager.upgrade_model("non-existent-model", None).await;

    // Assert it returned an error
    assert!(result.is_err());

    let err_msg = format!("{}", result.unwrap_err());
    assert!(err_msg.contains("not found"));

    shutdown_tx.send(()).unwrap();
}

#[tokio::test]
async fn test_load_and_unload_model() {
    // Start the mock server
    let (server_uri, shutdown_tx) = start_mock_server().await;

    // Create a manager with the mock server URI
    let mut manager = FoundryLocalManager::with_test_uri(&server_uri).await;

    // Test loading a model
    let model_info = manager.load_model("phi-4-mini", Some(300)).await.unwrap();
    assert_eq!(model_info.id, "Phi-4-mini-instruct-generic-cpu:1");

    // Verify the model is loaded
    let loaded_models = manager.list_loaded_models().await.unwrap();
    assert_eq!(loaded_models.len(), 1);
    assert_eq!(loaded_models[0].id, "Phi-4-mini-instruct-generic-cpu:1");

    // Test unloading the model
    manager.unload_model("phi-4-mini", false).await.unwrap();

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
