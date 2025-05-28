# Foundry Local Rust SDK

A Rust SDK for interacting with the Microsoft Foundry Local service. This SDK allows you to manage and use AI models locally on your device.

## Features
- Start and manage the Foundry Local service
- Download models from the Foundry catalog
- Load and unload models
- List available, cached, and loaded models
- Interact with loaded models using a simple API

## Usage

```rust
use foundry_local::FoundryLocalManager;
use anyhow::Result;

#[tokio::main]
async fn main() -> Result<()> {
    // Create a FoundryLocalManager instance with default options
    let manager = FoundryLocalManager::new("phi-3.5-mini", true).await?;
    
    // Use the OpenAI compatible API to interact with the model
    let client = reqwest::Client::new();
    let response = client.post(&format!("{}/chat/completions", manager.endpoint()))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("Bearer {}", manager.api_key()))
        .json(&serde_json::json!({
            "model": manager.get_model_info("phi-3.5-mini").await?.id,
            "messages": [{"role": "user", "content": "What is the golden ratio?"}],
        }))
        .send()
        .await?;
    
    let result = response.json::<serde_json::Value>().await?;
    println!("{}", result["choices"][0]["message"]["content"]);
    
    Ok(())
}
```

## Installation

Add the following to your `Cargo.toml`:

```toml
[dependencies]
foundry-local = "0.1.0"
```

## Requirements

- Foundry Local must be installed and available on the PATH
- Rust 1.70.0 or later

## License

Licensed under the MIT License. 