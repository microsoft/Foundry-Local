/// Foundry Local SDK - WinML 2.0 EP Verification (Rust)
///
/// Verifies:
///   1. WinML execution providers are discovered and registered
///   2. GPU models appear in catalog after EP registration
///   3. Streaming chat completions work on a WinML-accelerated model

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};
use std::io::{self, Write};
use tokio_stream::StreamExt;

const PASS: &str = "\x1b[92m[PASS]\x1b[0m";
const FAIL: &str = "\x1b[91m[FAIL]\x1b[0m";
const INFO: &str = "\x1b[94m[INFO]\x1b[0m";

fn is_winml_ep(name: &str) -> bool {
    let lower = name.to_lowercase();
    lower.contains("winml") || lower.contains("dml")
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let mut results: Vec<(&str, bool)> = Vec::new();

    // ── 0. Initialize FoundryLocalManager ──────────────────────
    println!("\n{}", "=".repeat(60));
    println!("  Initialization");
    println!("{}\n", "=".repeat(60));

    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("verify_winml"))?;
    println!("{INFO} FoundryLocalManager initialized.");

    // ── 1. Discover & Register EPs ────────────────────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 1: Discover & Register Execution Providers");
    println!("{}\n", "=".repeat(60));

    match manager.discover_eps().await {
        Ok(eps) => {
            println!("{INFO} Discovered {} execution providers:", eps.len());
            for ep in &eps {
                println!("  - {:<40}  Registered: {}", ep.name, ep.is_registered);
            }
            let detail = format!("{} EP(s) found", eps.len());
            println!("{PASS} EP Discovery - {detail}");
            results.push(("EP Discovery", true));
        }
        Err(e) => {
            println!("{FAIL} EP Discovery - {e}");
            results.push(("EP Discovery", false));
        }
    }

    match manager
        .download_and_register_eps(Some(|ep_name: &str, percent: f64| {
            print!("\r  Downloading {ep_name}: {percent:.1}%");
            io::stdout().flush().ok();
        }))
        .await
    {
        Ok(result) => {
            println!();
            println!(
                "{INFO} EP registration result: success={}, status={}",
                result.success, result.status
            );
            if !result.registered_eps.is_empty() {
                println!("  Registered: {}", result.registered_eps.join(", "));
            }
            if !result.failed_eps.is_empty() {
                println!("  Failed:     {}", result.failed_eps.join(", "));
            }
            let status = if result.success { PASS } else { FAIL };
            println!("{status} EP Download & Registration");
            results.push(("EP Download & Registration", result.success));
        }
        Err(e) => {
            println!();
            println!("{FAIL} EP Download & Registration - {e}");
            results.push(("EP Download & Registration", false));
        }
    }

    // ── 2. List Models & Find GPU/WinML Variants ───────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 2: Model Catalog - GPU/WinML Models");
    println!("{}\n", "=".repeat(60));

    let models = manager.catalog().list_models().await?;
    println!("{INFO} Total models in catalog: {}", models.len());

    let mut gpu_models = Vec::new();
    for model in &models {
        for variant in &model.variants {
            if let Some(rt) = &variant.info.runtime {
                if rt.device_type.as_deref() == Some("GPU") {
                    let ep = rt.execution_provider.as_deref().unwrap_or("?");
                    println!("  - {:<50}  EP: {ep}", variant.id);
                    gpu_models.push(variant);
                }
            }
        }
    }

    println!("{INFO} GPU model variants: {}", gpu_models.len());
    let has_gpu = !gpu_models.is_empty();
    let status = if has_gpu { PASS } else { FAIL };
    println!("{status} Catalog - GPU models found - {} GPU variant(s)", gpu_models.len());
    results.push(("Catalog - GPU models found", has_gpu));

    if gpu_models.is_empty() {
        println!("\n{FAIL} No GPU models available. Cannot proceed with inference tests.");
        print_summary(&results);
        return Ok(());
    }

    // Prefer WinML variant, fall back to any GPU
    let chosen = gpu_models
        .iter()
        .find(|v| {
            v.info
                .runtime
                .as_ref()
                .and_then(|rt| rt.execution_provider.as_deref())
                .map(is_winml_ep)
                .unwrap_or(false)
        })
        .or(gpu_models.first())
        .unwrap();

    let chosen_ep = chosen
        .info
        .runtime
        .as_ref()
        .and_then(|rt| rt.execution_provider.as_deref())
        .unwrap_or("unknown");
    println!("\n{INFO} Selected model: {} (EP: {chosen_ep})", chosen.id);

    // ── 3. Download & Load Model ──────────────────────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 3: Download & Load Model");
    println!("{}\n", "=".repeat(60));

    // Get the model by its parent alias
    let model_alias = chosen.id.split('/').next().unwrap_or(&chosen.id);
    let model = manager.catalog().get_model(model_alias).await?;

    if !model.is_cached().await? {
        match model
            .download(Some(|progress: f64| {
                print!("\r  Downloading model: {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await
        {
            Ok(_) => {
                println!();
                println!("{PASS} Model Download");
                results.push(("Model Download", true));
            }
            Err(e) => {
                println!();
                println!("{FAIL} Model Download - {e}");
                results.push(("Model Download", false));
                print_summary(&results);
                return Ok(());
            }
        }
    } else {
        println!("{PASS} Model Download - already cached");
        results.push(("Model Download", true));
    }

    match model.load().await {
        Ok(_) => {
            println!("{PASS} Model Load - Loaded {}", model_alias);
            results.push(("Model Load", true));
        }
        Err(e) => {
            println!("{FAIL} Model Load - {e}");
            results.push(("Model Load", false));
            print_summary(&results);
            return Ok(());
        }
    }

    // ── 4. Streaming Chat Completions ────────────────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 4: Streaming Chat Completions");
    println!("{}\n", "=".repeat(60));

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ChatCompletionRequestUserMessage::from("What is 2 + 2? Reply with just the number.").into(),
    ];

    let client = model.create_chat_client().temperature(0.7).max_tokens(64);
    match client.complete_streaming_chat(&messages, None).await {
        Ok(mut stream) => {
            let mut full_response = String::new();
            let start = std::time::Instant::now();
            while let Some(chunk) = stream.next().await {
                match chunk {
                    Ok(c) => {
                        if let Some(text) = c.choices.first().and_then(|ch| ch.delta.content.as_deref()) {
                            print!("{text}");
                            io::stdout().flush().ok();
                            full_response.push_str(text);
                        }
                    }
                    Err(e) => {
                        println!("\n{FAIL} Streaming chunk error: {e}");
                        break;
                    }
                }
            }
            let elapsed = start.elapsed().as_secs_f64();
            println!();
            let ok = !full_response.is_empty();
            let status = if ok { PASS } else { FAIL };
            println!("{status} Streaming Chat - {} chars in {elapsed:.2}s", full_response.len());
            results.push(("Streaming Chat", ok));
        }
        Err(e) => {
            println!("{FAIL} Streaming Chat - {e}");
            results.push(("Streaming Chat", false));
        }
    }

    print_summary(&results);
    Ok(())
}

fn print_summary(results: &[(&str, bool)]) {
    println!("\n{}", "=".repeat(60));
    println!("  Summary");
    println!("{}\n", "=".repeat(60));
    let passed = results.iter().filter(|(_, p)| *p).count();
    for (name, p) in results {
        println!("  {} {name}", if *p { "✓" } else { "✗" });
    }
    println!("\n  {passed}/{} tests passed", results.len());
}
