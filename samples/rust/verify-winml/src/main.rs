/// Foundry Local SDK - WinML 2.0 EP Verification (Rust)
///
/// Verifies:
///   1. Execution providers are discovered and registered
///   2. Accelerated models appear in catalog after EP registration
///   3. Streaming chat completions work on an accelerated model

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, DeviceType, FoundryLocalConfig,
    FoundryLocalManager, Model,
};
use std::io::{self, Write};
use tokio_stream::StreamExt;

const PASS: &str = "\x1b[92m[PASS]\x1b[0m";
const FAIL: &str = "\x1b[91m[FAIL]\x1b[0m";
const INFO: &str = "\x1b[94m[INFO]\x1b[0m";
const WARN: &str = "\x1b[93m[WARN]\x1b[0m";

fn is_accelerated_variant(model: &Model) -> bool {
    model.info()
        .runtime
        .as_ref()
        .map(|rt| matches!(rt.device_type, DeviceType::GPU | DeviceType::NPU))
        .unwrap_or(false)
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

    let eps = match manager.discover_eps() {
        Ok(eps) => {
            println!("{INFO} Discovered {} execution providers:", eps.len());
            for ep in &eps {
                println!("  - {:<40}  Registered: {}", ep.name, ep.is_registered);
            }

            let detail = format!("{} EP(s) found", eps.len());
            println!("{PASS} EP Discovery - {detail}");
            results.push(("EP Discovery", true));
            eps
        }
        Err(e) => {
            println!("{FAIL} EP Discovery - {e}");
            results.push(("EP Discovery", false));
            Vec::new()
        }
    };

    if eps.is_empty() {
        let detail = "No execution providers discovered on this machine";
        println!("{FAIL} EP Download & Registration - {detail}");
        println!("\n{FAIL} {detail}.");
        results.push(("EP Download & Registration", false));
        print_summary(&results);
        return Ok(());
    }

    match manager.download_and_register_eps_with_progress(None, {
        let mut last_progress_ep: Option<String> = None;
        let mut last_progress_percent = -1.0f64;

        move |ep_name: &str, percent: f64| {
            if last_progress_ep
                .as_ref()
                .map(|current| current != ep_name || percent < last_progress_percent)
                .unwrap_or(false)
            {
                println!();
            }

            last_progress_ep = Some(ep_name.to_string());
            last_progress_percent = percent;
            print!("\r  Downloading {ep_name}: {percent:.1}%");
            io::stdout().flush().ok();
        }
    }).await {
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

            let download_ok = result.success || !result.registered_eps.is_empty();
            let status = if download_ok { PASS } else { FAIL };
            let detail = if download_ok && !result.registered_eps.is_empty() {
                format!("{} EP(s) registered", result.registered_eps.len())
            } else {
                result.status.clone()
            };
            println!("{status} EP Download & Registration - {detail}");
            results.push(("EP Download & Registration", download_ok));

            if !download_ok {
                print_summary(&results);
                return Ok(());
            }
        }
        Err(e) => {
            println!();
            println!("{FAIL} EP Download & Registration - {e}");
            results.push(("EP Download & Registration", false));
            print_summary(&results);
            return Ok(());
        }
    }

    // ── 2. List Models & Find Accelerated Variants ────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 2: Model Catalog - Accelerated Models");
    println!("{}\n", "=".repeat(60));

    let models = manager.catalog().get_models().await?;
    println!("{INFO} Total models in catalog: {}", models.len());

    let mut accelerated_variants = Vec::new();
    for model in &models {
        for variant in model.variants() {
            if is_accelerated_variant(variant.as_ref()) {
                let device = variant
                    .info()
                    .runtime
                    .as_ref()
                    .map(|rt| format!("{:?}", rt.device_type))
                    .unwrap_or_else(|| "?".to_string());
                let ep = variant
                    .info()
                    .runtime
                    .as_ref()
                    .map(|rt| rt.execution_provider.as_str())
                    .unwrap_or("?");
                println!(
                    "  - {:<50}  Device: {:<3}  EP: {}",
                    variant.id(),
                    device,
                    ep
                );
                accelerated_variants.push(variant);
            }
        }
    }

    println!("{INFO} Accelerated model variants: {}", accelerated_variants.len());
    let has_accelerated_models = !accelerated_variants.is_empty();
    let status = if has_accelerated_models { PASS } else { FAIL };
    println!(
        "{status} Catalog - Accelerated models found - {} accelerated variant(s)",
        accelerated_variants.len()
    );
    results.push(("Catalog - Accelerated models found", has_accelerated_models));

    if accelerated_variants.is_empty() {
        println!("\n{FAIL} No accelerated model variants are available.");
        println!("{WARN} Ensure the system has a compatible accelerator and matching model variants installed.");
        print_summary(&results);
        return Ok(());
    }

    let chosen = accelerated_variants
        .first()
        .cloned()
        .expect("accelerated_variants is not empty");
    let chosen_ep = chosen
        .info()
        .runtime
        .as_ref()
        .map(|rt| rt.execution_provider.as_str())
        .unwrap_or("unknown");
    println!("\n{INFO} Selected model: {} (EP: {chosen_ep})", chosen.id());

    // ── 3. Download & Load Model ──────────────────────────────
    println!("\n{}", "=".repeat(60));
    println!("  Step 3: Download & Load Model");
    println!("{}\n", "=".repeat(60));

    let model = manager.catalog().get_model(chosen.alias()).await?;
    model.select_variant_by_id(chosen.id())?;

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
            println!("{PASS} Model Load - Loaded {}", chosen.id());
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
                        if let Some(text) = c
                            .choices
                            .first()
                            .and_then(|ch| ch.delta.content.as_deref())
                        {
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
            println!(
                "{status} Streaming Chat - {} chars in {elapsed:.2}s",
                full_response.len()
            );
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
