use std::io::{self, Write};

use foundry_local_sdk::{EmbeddingsSession, FoundryLocalConfig, FoundryLocalManager};

const MODEL_ALIAS: &str = "qwen3-embedding-0.6b";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    let model = manager.catalog().get_model(MODEL_ALIAS).await?;

    if !model.is_cached().await? {
        model
            .download(Some(|progress| {
                print!("\rDownloading model: {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    model.load().await?;
    let session = EmbeddingsSession::new(&model).await?;

    let vector = session
        .embed("Foundry Local runs AI models on your device.")
        .await?;
    println!("Single embedding dimensions: {}", vector.len());
    println!("First values: {:?}", &vector[..vector.len().min(5)]);

    let vectors = session
        .embed_batch(vec![
            "Rust provides memory safety.".to_string(),
            "Local inference keeps data on device.".to_string(),
        ])
        .await?;
    println!("Batch embeddings: {}", vectors.len());

    drop(session);
    model.unload().await?;
    manager.shutdown()?;
    Ok(())
}
