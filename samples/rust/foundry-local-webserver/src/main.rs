// <complete_code>
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//! Foundry Local Web Server example.
//!
//! Demonstrates how to start a local OpenAI-compatible web server using the
//! Foundry Local SDK, then call it with a standard HTTP client. This is useful
//! when you want to use the OpenAI REST API directly or integrate with tools
//! that expect an OpenAI-compatible endpoint.

// <imports>
use std::io::{self, Write};

use serde_json::json;

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
// </imports>

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // ── 1. Initialise the SDK ────────────────────────────────────────────
    // <init>
    println!("Initializing Foundry Local SDK...");
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    println!("✓ SDK initialized");
    // </init>

    // ── 2. Download and load a model ─────────────────────────────────────
    // <model_setup>
    let model_alias = "qwen2.5-0.5b";
    let model = manager.catalog().get_model(model_alias).await?;

    if !model.is_cached().await? {
        print!("Downloading model {model_alias}...");
        model
            .download(Some(move |progress: &str| {
                print!("\rDownloading model... {progress}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    print!("Loading model {model_alias}...");
    model.load().await?;
    println!("done.");
    // </model_setup>

    // <server_setup>
    // ── 3. Start the web service─────────────────────────────────────────
    print!("Starting web service...");
    manager.start_web_service().await?;
    println!("done.");

    let urls = manager.urls()?;
    let endpoint = urls
        .first()
        .expect("Web service did not return an endpoint");
    println!("Web service listening on: {endpoint}");

    // ── 4. Use the OpenAI-compatible REST API with streaming ────────────
    // Any HTTP client (or OpenAI SDK) can now talk to this endpoint.
    let client = reqwest::Client::new();
    let base_url = endpoint.trim_end_matches('/');

    let mut response = client
        .post(format!("{base_url}/v1/chat/completions"))
        .json(&json!({
            "model": model.id(),
            "messages": [
                { "role": "user", "content": "Why is the sky blue?" }
            ],
            "stream": true
        }))
        .send()
        .await?;

    print!("[ASSISTANT]: ");
    while let Some(chunk) = response.chunk().await? {
        let text = String::from_utf8_lossy(&chunk);
        for line in text.lines() {
            let line = line.trim();
            if let Some(data) = line.strip_prefix("data: ") {
                if data == "[DONE]" {
                    break;
                }
                if let Ok(parsed) = serde_json::from_str::<serde_json::Value>(data) {
                    if let Some(content) = parsed
                        .pointer("/choices/0/delta/content")
                        .and_then(|v| v.as_str())
                    {
                        print!("{content}");
                        io::stdout().flush().ok();
                    }
                }
            }
        }
    }
    println!();
    // </server_setup>

    // ── 5. Clean up ──────────────────────────────────────────────────────
    println!("\nStopping web service...");
    manager.stop_web_service().await?;

    println!("Unloading model...");
    model.unload().await?;

    println!("✓ Done.");
    Ok(())
}
// </complete_code>
