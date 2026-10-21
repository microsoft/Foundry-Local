use std::io::{self, Write};

use foundry_local_sdk::{
    ChatSession, FoundryLocalConfig, FoundryLocalManager, Item, Request, RequestOptions,
    SearchOptions,
};

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

    let result = manager
        .download_and_register_eps_with_progress(None, |name, progress| {
            print!("\rRegistering {name}: {progress:.1}%");
            io::stdout().flush().ok();
        })
        .await?;
    println!("\n{}", result.status);

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
        "Why is the sky blue? Answer in two sentences.",
    )])])
    .with_options(RequestOptions {
        search: SearchOptions {
            temperature: Some(0.0),
            max_output_tokens: Some(128),
            ..Default::default()
        },
        ..Default::default()
    });

    let response = session.process_request(request).await?;
    for item in &response.items {
        if let Some(message) = item.as_message() {
            println!("Assistant: {}", message.text());
        }
    }

    drop(session);
    model.unload().await?;
    manager.shutdown()?;
    Ok(())
}
