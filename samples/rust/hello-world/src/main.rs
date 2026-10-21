use std::io::{self, Write};

use foundry_local_sdk::{
    ChatSession, FoundryLocalConfig, FoundryLocalManager, Item, Request, TextKind,
};
use tokio_stream::StreamExt;

const MODEL_ALIAS: &str = "qwen2.5-0.5b";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;

    let execution_providers = manager.discover_eps()?;
    println!(
        "Discovered {} execution provider(s).",
        execution_providers.len()
    );
    for ep in &execution_providers {
        println!(
            "  {} ({})",
            ep.name,
            if ep.is_registered {
                "registered"
            } else {
                "not registered"
            }
        );
    }

    if execution_providers.iter().any(|ep| !ep.is_registered) {
        let result = manager
            .download_and_register_eps_with_progress(None, |name, progress| {
                print!("\rRegistering {name}: {progress:.1}%");
                io::stdout().flush().ok();
            })
            .await?;
        println!("\n{}", result.status);
    }

    let catalog = manager.catalog();
    println!("Catalog: {}", catalog.name());
    let model = catalog.get_model(MODEL_ALIAS).await?;
    println!("Model: {} ({})", model.alias(), model.id());

    if !model.is_cached().await? {
        model
            .download(Some(|progress| {
                print!("\rDownloading model: {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    } else {
        println!("Using cached model assets.");
    }

    model.load().await?;
    let session = ChatSession::new(&model).await?;
    let request = Request::from_items(vec![Item::user_message(vec![Item::text(
        "In one sentence, what can I build with Foundry Local?",
    )])]);

    print!("Assistant: ");
    let mut stream = session.process_streaming_request(request);
    while let Some(item) = stream.next().await {
        if let Item::Text {
            text,
            kind: TextKind::Default,
        } = item?
        {
            print!("{text}");
            io::stdout().flush().ok();
        }
    }
    println!();

    drop(session);
    model.unload().await?;
    manager.shutdown()?;
    Ok(())
}
