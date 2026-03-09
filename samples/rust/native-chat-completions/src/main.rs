// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

use std::io::{self, Write};

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};
use tokio_stream::StreamExt;

const ALIAS: &str = "qwen2.5-0.5b";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Native Chat Completions");
    println!("=======================\n");

    // ── 1. Initialise the manager ────────────────────────────────────────
    let manager = FoundryLocalManager::create(FoundryLocalConfig {
        app_name: "foundry_local_samples".into(),
        ..Default::default()
    })?;

    // ── 2. Pick a model and ensure it is downloaded ──────────────────────
    let model = manager.catalog().get_model(ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: &str| {
                print!("\r  {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");

    // ── 3. Create a chat client ──────────────────────────────────────────
    let mut client = model.create_chat_client();
    client.temperature(0.7).max_tokens(256);

    // ── 4. Non-streaming chat completion ─────────────────────────────────
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

    // ── 5. Streaming chat completion ─────────────────────────────────────
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
    stream.close().await?;
    println!("\n");

    // ── 6. Unload the model ──────────────────────────────────────────────
    println!("Unloading model...");
    model.unload().await?;
    println!("Done.");

    Ok(())
}
