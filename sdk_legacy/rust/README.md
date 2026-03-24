# Foundry Local Rust SDK

A Rust SDK for interacting with the Microsoft Foundry Local service. This SDK allows you to manage and use AI models locally on your device.

## Features
- Start and manage the Foundry Local service
- Download models from the Foundry catalog
- Load and unload models
- List available, cached, and loaded models
- Interact with loaded models using a simple API

## Usage

### Prerequisites

To use this SDK, ensure you have the following prerequisites:

- Foundry Local must be installed and available on the PATH
- Rust 1.70.0 or later

### Create a new Rust project:

```bash
cargo new hello-foundry-local
cd hello-foundry-local
```

### Install crates

Install the following Rust crates using Cargo:

```bash
cargo add foundry-local anyhow env_logger serde_json
cargo add reqwest --features json
cargo add tokio --features full
```

Alternatively, you can add these dependencies manually to your `Cargo.toml` file.

```toml
[dependencies]
anyhow = "1.0.98"
env_logger = "0.11.8"
foundry-local = "0.1.0"
reqwest = { version = "0.12.19", features = ["json"] }
serde_json = "1.0.140"
tokio = { version = "1.45.1", features = ["full"] }
```

### Update the `main.rs` file

Replace the contents of `src/main.rs` with the following code to create a simple application that interacts with the Foundry Local service:

```rust
use foundry_local::FoundryLocalManager;
use anyhow::Result;

#[tokio::main]
async fn main() -> Result<()> {
    // Create a FoundryLocalManager instance with default options
    let mut manager = FoundryLocalManager::builder()
        .alias_or_model_id("phi-3.5-mini") // Specify the model to use   
        .bootstrap(true) // Start the service if not running
        .build()
        .await?;
    
    // Use the OpenAI compatible API to interact with the model
    let client = reqwest::Client::new();
    let endpoint = manager.endpoint()?;
    let response = client.post(&format!("{}/chat/completions", endpoint))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("Bearer {}", manager.api_key()))
        .json(&serde_json::json!({
            "model": manager.get_model_info("phi-3.5-mini", true).await?.id,
            "messages": [{"role": "user", "content": "What is the golden ratio?"}],
        }))
        .send()
        .await?;

    let result = response.json::<serde_json::Value>().await?;
    println!("{}", result["choices"][0]["message"]["content"]);
    
    Ok(())
}
```

### Run the application

To run the application, execute the following command in your terminal:

```bash
cargo run
```

## License

Licensed under the MIT License. 