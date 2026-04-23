// <complete_code>
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// <imports>
use std::io::{self, Write};

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
// </imports>

const ALIAS: &str = "qwen3-embedding-0.6b";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Native Embeddings");
    println!("=================\n");

    // ── 1. Initialise the manager ────────────────────────────────────────
    // <init>
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    // </init>

    // Discover available execution providers and their registration status.
    let eps = manager.discover_eps()?;
    let max_name_len = 30;
    println!("Available execution providers:");
    println!("  {:<width$}  Registered", "Name", width = max_name_len);
    println!("  {:─<width$}  ──────────", "", width = max_name_len);
    for ep in &eps {
        println!("  {:<width$}  {}", ep.name, ep.is_registered, width = max_name_len);
    }

    // Download and register all execution providers.
    println!("\nDownloading execution providers:");
    if !eps.is_empty() {
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
                    print!("\r  {:<width$}  {:5.1}%", ep_name, percent, width = max_name_len);
                    io::stdout().flush().ok();
                }
            })
            .await?;
        println!();
    } else {
        println!("No execution providers to download.");
    }

    // ── 2. Pick a model and ensure it is downloaded ─────────────────────
    // <model_setup>
    let model = manager.catalog().get_model(ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: f64| {
                print!("\r  {progress:.1}%");
                std::io::Write::flush(&mut std::io::stdout()).ok();
            }))
            .await?;
        println!();
    }

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");
    // </model_setup>

    // ── 3. Create an embedding client ───────────────────────────────────
    // <embedding_client>
    let client = model.create_embedding_client();
    // </embedding_client>

    // ── 4. Single embedding ─────────────────────────────────────────────
    // <single_embedding>
    println!("--- Single Embedding ---");
    let response = client
        .generate_embedding("The quick brown fox jumps over the lazy dog")
        .await?;

    let embedding = &response.data[0].embedding;
    println!("Dimensions: {}", embedding.len());
    println!(
        "First 5 values: {:?}",
        &embedding[..5]
    );
    // </single_embedding>

    // ── 5. Batch embeddings ─────────────────────────────────────────────
    // <batch_embedding>
    println!("\n--- Batch Embeddings ---");
    let batch_response = client
        .generate_embeddings(&[
            "Machine learning is a subset of artificial intelligence",
            "The capital of France is Paris",
            "Rust is a systems programming language",
        ])
        .await?;

    println!("Number of embeddings: {}", batch_response.data.len());
    for (i, data) in batch_response.data.iter().enumerate() {
        println!("  [{i}] Dimensions: {}", data.embedding.len());
    }
    // </batch_embedding>

    // ── 6. Unload the model ─────────────────────────────────────────────
    // <cleanup>
    println!("\nUnloading model...");
    model.unload().await?;
    println!("Done.");
    // </cleanup>

    Ok(())
}
// </complete_code>
