#[cfg(feature = "integration-tests")]
mod integration_tests {
    use foundry_local::{
        FoundryLocalManager,
        models::ExecutionProvider,
    };
    use std::time::Duration;

    async fn setup_manager() -> FoundryLocalManager {
        FoundryLocalManager::new(None, None, None).await.expect("Failed to create manager")
    }

    #[tokio::test]
    async fn test_real_service_workflow() {
        // Create manager
        let mut manager = setup_manager().await;

        // List catalog models and get the first model ID
        let model_id = {
            let catalog_models = manager.list_catalog_models()
                .await
                .expect("Failed to list catalog models");
            
            assert!(!catalog_models.is_empty(), "No models found in catalog");
            
            let first_model = &catalog_models[0];
            println!("Testing with model: {} ({})", first_model.alias, first_model.id);
            first_model.id.clone()
        };

        // Download the model
//        let model_info = manager.download_model(&model_id, None, false)
//            .await
//            .expect("Failed to download model");
//        assert_eq!(model_info.id, model_id);

        // Load the model
        let model_info = manager.load_model(&model_id, Some(300))
            .await
            .expect("Failed to load model");
        assert_eq!(model_info.id, model_id);

                // Verify model is unloaded
        let loaded_models = manager.list_loaded_models()
            .await
            .expect("Failed to list loaded models");
        println!("\nLoaded models:");
        for model in &loaded_models {
            println!("- {} ({})", model.alias, model.id);
        }
        assert!(loaded_models.iter().any(|m| m.id == model_id));


        // Get the endpoint before making the request
        let endpoint = manager.endpoint().expect("Failed to get endpoint");

        // Use the OpenAI compatible API to interact with the model
        let client = reqwest::Client::new();
        let response = client.post(&format!("{}/chat/completions", endpoint))
            .json(&serde_json::json!({
                "model": model_info.id,
                "messages": [{"role": "user", "content": "What is 2+2?"}],
            }))
            .send()
            .await
            .expect("Failed to send request");
        
        // Parse and display the response
        let result = response.json::<serde_json::Value>().await.expect("Failed to parse response");
        if let Some(content) = result["choices"][0]["message"]["content"].as_str() {
            println!("\nResponse:\n{}", content);
        } else {
            println!("\nError: Failed to extract response content from API result");
            println!("Full API response: {}", result);
        }

        // Unload the model
        println!("Unloading model...{}", model_id);
        manager.unload_model(&model_id, true)
            .await
            .expect("Failed to unload model");

        // Verify model is unloaded
        let loaded_models = manager.list_loaded_models()
            .await
            .expect("Failed to list loaded models");
        assert!(!loaded_models.iter().any(|m| m.id == model_id));
    }
} 