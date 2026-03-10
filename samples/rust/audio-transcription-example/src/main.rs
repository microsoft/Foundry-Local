// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

use std::env;
use std::io::{self, Write};

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
use tokio_stream::StreamExt;

const ALIAS: &str = "whisper-tiny";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Audio Transcription Example");
    println!("===========================\n");

    // Accept an audio file path as a CLI argument.
    let audio_path = env::args().nth(1).unwrap_or_else(|| {
        eprintln!("Usage: cargo run -- <path-to-audio.wav>");
        std::process::exit(1);
    });

    // ── 1. Initialise the manager ────────────────────────────────────────
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;

    // ── 2. Pick the whisper model and ensure it is downloaded ────────────
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

    // ── 3. Create an audio client ────────────────────────────────────────
    let audio_client = model.create_audio_client();

    // ── 4. Non-streaming transcription ───────────────────────────────────
    println!("--- Non-streaming transcription ---");
    let result = audio_client.transcribe(&audio_path).await?;
    println!("Transcription: {}", result.text);

    // ── 5. Streaming transcription ───────────────────────────────────────
    println!("--- Streaming transcription ---");
    print!("Transcription: ");
    let mut stream = audio_client.transcribe_streaming(&audio_path).await?;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        print!("{}", chunk.text);
        io::stdout().flush().ok();
    }
    stream.close().await?;
    println!("\n");

    // ── 6. Unload the model ──────────────────────────────────────────────
    println!("Unloading model...");
    model.unload().await?;
    println!("Done.");

    Ok(())
}
