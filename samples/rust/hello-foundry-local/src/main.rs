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

    // Create a FoundryLocalManager instance with default options
    // This will start the Foundry service if not running
    println!("\nInitializing Foundry Local manager...");
    let mut manager = FoundryLocalManager::new(None, None, None).await?;

    // List all the models in the catalog
    println!("\nAvailable models in catalog:");
    let models = manager.list_catalog_models().await?;
    for model in models {
        println!("- {model}");
    }

    // List available models in the local cache
    println!("\nAvailable models in local cache:");
    let models = manager.list_cached_models().await?;
    for model in models {
        println!("- {model}");
    }

    // Get the model information
    let model_info = manager.get_model_info("phi-4-mini", true).await?;
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
