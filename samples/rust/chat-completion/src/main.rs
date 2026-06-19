// <complete_code>
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// <imports>
use std::io::{self, Write};

use serde_json::json;

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};
// </imports>

const ALIAS: &str = "qwen2.5-0.5b";
const SYSTEM_PROMPT: &str = "You are a helpful assistant.";
const USER_PROMPT: &str = "What is Rust's ownership model?";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Native + Web Server Chat Completions");
    println!("====================================\n");

    // ── 1. Initialise the manager ────────────────────────────────────────
    // <init>
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    // </init>

    // Download and register all execution providers.
    manager
        .download_and_register_eps_with_progress(None, {
            let mut current_ep = String::new();
            move |ep_name: &str, percent: f64| {
                if ep_name != current_ep {
                    if !current_ep.is_empty() {
                        println!();
                    }
                    current_ep = ep_name.to_string();
                }
                print!("\r  {:<30}  {:5.1}%", ep_name, percent);
                io::stdout().flush().ok();
            }
        })
        .await?;
    println!();

    // ── 2. Pick a model and ensure it is downloaded ─────────────────────
    // <model_setup>
    let model = manager.catalog().get_model(ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: f64| {
                print!("\r  {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");
    // </model_setup>

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(SYSTEM_PROMPT).into(),
        ChatCompletionRequestUserMessage::from(USER_PROMPT).into(),
    ];

    println!("==================================================");
    println!("  1. Native in-process chat completion");
    println!("==================================================");
    println!("Prompt: {USER_PROMPT}\n");

    // ── 3. Create a chat client ─────────────────────────────────────────
    // <chat_client>
    let client = model.create_chat_client().temperature(0.7).max_tokens(256);
    // </chat_client>

    // ── 4. Native chat completion ───────────────────────────────────────
    // <chat_completion>
    let response = client.complete_chat(&messages, None).await?;
    if let Some(choice) = response.choices.first() {
        if let Some(ref content) = choice.message.content {
            println!("Assistant: {content}");
        }
    }
    // </chat_completion>

    println!("\n==================================================");
    println!("  2. Local web server /v1/chat/completions");
    println!("==================================================");
    println!("Prompt: {USER_PROMPT}\n");

    // <server_setup>
    print!("Starting web service...");
    manager.start_web_service().await?;
    println!("done.");

    let web_result = run_web_chat_completion(manager, model.id()).await;

    println!("\nStopping web service...");
    let stop_result = manager.stop_web_service().await;

    // ── 5. Unload the model ─────────────────────────────────────────────
    // <cleanup>
    println!("Unloading model...");
    let unload_result = model.unload().await;
    // </cleanup>

    web_result?;
    stop_result?;
    unload_result?;

    println!("Done.");
    Ok(())
}

async fn run_web_chat_completion(
    manager: &FoundryLocalManager,
    model_id: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let urls = manager.urls()?;
    let endpoint = urls
        .first()
        .expect("Web service did not return an endpoint");
    let base_url = endpoint.trim_end_matches('/');
    println!("Web service listening on: {base_url}");

    let client = reqwest::Client::new();
    let mut response = client
        .post(format!("{base_url}/v1/chat/completions"))
        .json(&json!({
            "model": model_id,
            "messages": [
                { "role": "system", "content": SYSTEM_PROMPT },
                { "role": "user", "content": USER_PROMPT }
            ],
            "stream": true
        }))
        .send()
        .await?
        .error_for_status()?;

    print!("Assistant: ");
    io::stdout().flush().ok();

    let mut buffer = String::new();
    while let Some(chunk) = response.chunk().await? {
        buffer.push_str(&String::from_utf8_lossy(&chunk));
        while let Some(newline) = buffer.find('\n') {
            let line = buffer[..newline].trim_end().to_string();
            buffer.drain(..=newline);

            let Some(data) = line.strip_prefix("data: ") else {
                continue;
            };
            if data == "[DONE]" {
                println!();
                return Ok(());
            }

            if let Ok(parsed) = serde_json::from_str::<serde_json::Value>(data) {
                if let Some(content) = parsed
                    .pointer("/choices/0/delta/content")
                    .and_then(|value| value.as_str())
                {
                    print!("{content}");
                    io::stdout().flush().ok();
                }
            }
        }
    }

    println!();
    Ok(())
}
// </complete_code>
