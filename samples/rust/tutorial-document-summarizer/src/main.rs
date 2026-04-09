// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig,
    FoundryLocalManager,
};
use std::io::{self, Write};
use std::path::Path;
use std::{env, fs};
// </imports>

async fn summarize_file(
    client: &foundry_local_sdk::openai::ChatClient,
    file_path: &Path,
    system_prompt: &str,
) -> anyhow::Result<()> {
    let content = fs::read_to_string(file_path)?;
    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(system_prompt)
            .into(),
        ChatCompletionRequestUserMessage::from(content.as_str())
            .into(),
    ];

    let response =
        client.complete_chat(&messages, None).await?;
    let summary = response.choices[0]
        .message
        .content
        .as_deref()
        .unwrap_or("");
    println!("{}", summary);
    Ok(())
}

async fn summarize_directory(
    client: &foundry_local_sdk::openai::ChatClient,
    directory: &Path,
    system_prompt: &str,
) -> anyhow::Result<()> {
    let mut txt_files: Vec<_> = fs::read_dir(directory)?
        .filter_map(|entry| entry.ok())
        .filter(|entry| {
            entry
                .path()
                .extension()
                .map(|ext| ext == "txt")
                .unwrap_or(false)
        })
        .collect();

    txt_files.sort_by_key(|e| e.path());

    if txt_files.is_empty() {
        println!(
            "No .txt files found in {}",
            directory.display()
        );
        return Ok(());
    }

    for entry in &txt_files {
        let file_name = entry.file_name();
        println!(
            "--- {} ---",
            file_name.to_string_lossy()
        );
        summarize_file(
            client,
            &entry.path(),
            system_prompt,
        )
        .await?;
        println!();
    }

    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    // Initialize the Foundry Local SDK
    let manager = FoundryLocalManager::create(
        FoundryLocalConfig::new("doc-summarizer"),
    )?;

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

    // Select and load a model from the catalog
    let model = manager
        .catalog()
        .get_model("qwen2.5-0.5b")
        .await?;

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

    model.load().await?;
    println!("Model loaded and ready.\n");

    // Create a chat client
    let client = model
        .create_chat_client()
        .temperature(0.7)
        .max_tokens(512);
    // </init>

    // <summarization>
    let system_prompt = "Summarize the following document \
         into concise bullet points. Focus on the key \
         points and main ideas.";

    // <file_reading>
    let target = env::args()
        .nth(1)
        .unwrap_or_else(|| "document.txt".to_string());
    let target_path = Path::new(&target);
    // </file_reading>

    if target_path.is_dir() {
        summarize_directory(
            &client,
            target_path,
            system_prompt,
        )
        .await?;
    } else {
        let file_name = target_path
            .file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_else(|| target.clone());
        println!("--- {} ---", file_name);
        summarize_file(
            &client,
            target_path,
            system_prompt,
        )
        .await?;
    }
    // </summarization>

    // Clean up
    model.unload().await?;
    println!("\nModel unloaded. Done!");

    Ok(())
}
// </complete_code>
