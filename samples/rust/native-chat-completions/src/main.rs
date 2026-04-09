// <complete_code>
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// <imports>
use std::io::{self, Write};

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};
use tokio_stream::StreamExt;
// </imports>

const ALIAS: &str = "qwen2.5-0.5b";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Native Chat Completions");
    println!("=======================\n");

    // ── 1. Initialise the manager ────────────────────────────────────────
    // <init>
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    // </init>

    // Download and register all execution providers.
    let mut current_ep = String::new();
    manager
        .download_and_register_eps_with_progress(None, |ep_name: &str, percent: f64| {
            if ep_name != current_ep {
                if !current_ep.is_empty() {
                    println!();
                }
                current_ep = ep_name.to_string();
            }
            print!("\r  {:<30}  {:5.1}%", ep_name, percent);
            io::stdout().flush().ok();
        })
        .await?;
    if !current_ep.is_empty() {
        println!();
    }

    // ── 2. Pick a modeland ensure it is downloaded ──────────────────────
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

    // ── 3. Create a chat client──────────────────────────────────────────
    // <chat_client>
    let client = model.create_chat_client()
        .temperature(0.7)
        .max_tokens(256);
    // </chat_client>

    // ── 4. Non-streamingchat completion ─────────────────────────────────
    // <chat_completion>
    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("What is Rust's ownership model?").into(),
    ];

    println!("--- Non-streaming completion ---");
    let response = client.complete_chat(&messages, None).await?;
    if let Some(choice) = response.choices.first() {
        if let Some(ref content) = choice.message.content {
            println!("Assistant: {content}");
        }
    }
    // </chat_completion>

    // ── 5. Streamingchat completion ─────────────────────────────────────
    // <streaming>
    let stream_messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("Explain the borrow checker in two sentences.")
            .into(),
    ];

    println!("\n--- Streaming completion ---");
    print!("Assistant: ");
    let mut stream = client
        .complete_streaming_chat(&stream_messages, None)
        .await?;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        if let Some(choice) = chunk.choices.first() {
            if let Some(ref content) = choice.delta.content {
                print!("{content}");
                io::stdout().flush().ok();
            }
        }
    }
    println!("\n");
    // </streaming>

    // ── 6. Unloadthe model──────────────────────────────────────────────
    // <cleanup>
    println!("Unloading model...");
    model.unload().await?;
    println!("Done.");
    // </cleanup>

    Ok(())
}
// </complete_code>
