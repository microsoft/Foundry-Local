use anyhow::Result;
use foundry_local::FoundryLocalManager;

#[tokio::main]
async fn main() -> Result<()> {
    // Set up logging
    env_logger::init_from_env(
        env_logger::Env::default().filter_or(env_logger::DEFAULT_FILTER_ENV, "info"),
    );

    println!("Hello Foundry Local!");
    println!("===================");

    // For this example, we will use the "phi-3-mini-4k" model which is 2.181 GB in size.
    let model_to_use: &str = "phi-3-mini-4k";

    // Create a FoundryLocalManager instance using the builder pattern
    println!("\nInitializing Foundry Local manager...");
    let mut manager = FoundryLocalManager::builder()
        // Alternatively to the checks below, you can specify the model to use directly during bootstrapping
        // .alias_or_model_id(model_to_use)
        .bootstrap(true) // Start the service if not running
        .build()
        .await?;

    // List all the models in the catalog
    println!("\nAvailable models in catalog:");
    let models = manager.list_catalog_models().await?;
    let model_in_catalog = models.iter().any(|m| m.alias == model_to_use);
    for model in models {
        println!("- {model}");
    }
    // Check if the model is in the catalog
    if !model_in_catalog {
        println!("Model '{model_to_use}' not found in catalog. Exiting.");
        return Ok(());
    }

    // List available models in the local cache
    println!("\nAvailable models in local cache:");
    let models = manager.list_cached_models().await?;
    let model_in_cache = models.iter().any(|m| m.alias == model_to_use);
    for model in models {
        println!("- {model}");
    }

    // Check if the model is already cached and download if not
    if !model_in_cache {
        println!("Model '{model_to_use}' not found in local cache. Downloading...");
        // Download the model if not in cache
        // NOTE if you've bootstrapped with `alias_or_model_id`, you can use that directly and skip this check
        manager.download_model(model_to_use, None, false).await?;
        println!("Model '{model_to_use}' downloaded successfully.");
    }

    // Get the model information
    let model_info = manager.get_model_info(model_to_use, true).await?;
    println!("\nUsing model: {model_info}");

    // Build the prompt
    let prompt = "What is the golden ratio?";
    println!("\nPrompt: {prompt}");

    // Use the OpenAI compatible API to interact with the model
    let client = reqwest::Client::new();
    let response = client
        .post(format!("{}/chat/completions", manager.endpoint()?))
        .json(&serde_json::json!({
            "model": model_info.id,
            "messages": [{"role": "user", "content": prompt}],
        }))
        .send()
        .await?;

    // Parse and display the response
    let result = response.json::<serde_json::Value>().await?;
    if let Some(content) = result["choices"][0]["message"]["content"].as_str() {
        println!("\nResponse:\n{content}");
    } else {
        println!("\nError: Failed to extract response content from API result");
        println!("Full API response: {result}");
    }

    Ok(())
}
