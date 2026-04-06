//! Example: Cancelling a model download using Foundry Local Rust SDK.
//!
//! Demonstrates how to cancel a running model download using a shared
//! `Arc<AtomicBool>` flag.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalError, FoundryLocalManager};

type Result<T> = std::result::Result<T, FoundryLocalError>;

#[tokio::main]
async fn main() -> Result<()> {
    // ── 1. Initialise the manager ────────────────────────────────────────
    let config = FoundryLocalConfig::new("cancellable_download_example");
    let manager = FoundryLocalManager::create(config)?;

    // ── 2. Pick a model ──────────────────────────────────────────────────
    let model_alias = "phi-4-mini";
    let model = manager.catalog().get_model(model_alias).await?;

    if model.is_cached().await? {
        println!(
            "Model '{}' is already cached. Remove it first to test cancellation.",
            model.alias()
        );
        return Ok(());
    }

    // ── 3. Create a shared cancellation flag ─────────────────────────────
    let cancel_flag = Arc::new(AtomicBool::new(false));
    let cancel_flag_clone = Arc::clone(&cancel_flag);

    // ── 4. Spawn a task that cancels the download after 3 seconds ────────
    tokio::spawn(async move {
        tokio::time::sleep(std::time::Duration::from_secs(3)).await;
        println!("\nCancelling download...");
        cancel_flag_clone.store(true, Ordering::Relaxed);
    });

    // ── 5. Start the download with cancellation support ──────────────────
    println!("Starting download of '{}'…", model.alias());
    let result = model
        .download_cancellable(
            Some(|progress: &str| {
                println!("  {progress}");
            }),
            cancel_flag,
        )
        .await;

    match result {
        Ok(()) => println!("Download completed before cancellation took effect."),
        Err(e) => println!("Download was cancelled: {e}"),
    }

    Ok(())
}
