#[path = "ui.rs"]
mod ui;

use std::io::{self, Write};

use foundry_local_sdk::{
    ChatCompletionRequestAssistantMessage, ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage, ChatCompletionRequestUserMessage,
    FoundryLocalConfig, FoundryLocalManager,
};
use tokio_stream::StreamExt;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // ── Initialize ───────────────────────────────────────────────────────

    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry-local-playground"))?;

    // ── Discover & download execution providers ──────────────────────────

    ui::section("Execution Providers");

    let eps_raw = manager.discover_eps()?;
    let eps: Vec<ui::EpEntry> = eps_raw
        .iter()
        .map(|ep| ui::EpEntry {
            name: ep.name.clone(),
            is_registered: ep.is_registered,
        })
        .collect();

    ui::show_ep_table(&eps);

    let unregistered: Vec<&str> = eps_raw
        .iter()
        .filter(|ep| !ep.is_registered)
        .map(|ep| ep.name.as_str())
        .collect();

    if !unregistered.is_empty() {
        let cb = ui::ep_progress_callback(eps.clone());
        let result = manager
            .download_and_register_eps_with_progress(Some(&unregistered), cb)
            .await?;
        let failed: Vec<String> = result.failed_eps;
        ui::finalize_ep_table(&eps, &failed);
    }

    // ── Browse model catalog & pick a model ──────────────────────────────

    ui::section("Model Catalog");

    let catalog = manager.catalog();
    let mut models = catalog.get_models().await?;
    models.sort_by(|a, b| {
        let ca = a.info().cached;
        let cb = b.info().cached;
        cb.cmp(&ca).then_with(|| a.alias().cmp(b.alias()))
    });

    let mut catalog_rows: Vec<ui::CatalogRow> = Vec::new();
    let mut num: usize = 1;
    for (i, m) in models.iter().enumerate() {
        let info = m.info();
        let size_gb = info
            .file_size_mb
            .map(|s| format!("{:.1}", s as f64 / 1024.0))
            .unwrap_or_else(|| "?".to_string());
        let task = info.task.clone().unwrap_or_else(|| "?".to_string());

        let variants = m.variants();
        for (v_idx, v) in variants.iter().enumerate() {
            catalog_rows.push(ui::CatalogRow {
                num,
                alias: m.alias().to_string(),
                variant_id: v.id().to_string(),
                size_gb: size_gb.clone(),
                task: task.clone(),
                is_cached: v.info().cached,
                is_first_variant: v_idx == 0,
                model_idx: i,
            });
            num += 1;
        }
    }

    ui::show_catalog(&catalog_rows);

    let total = catalog_rows.len();
    let choice = ui::ask_user(&format!(
        "\n  Select a model [\x1b[36m1-{total}\x1b[0m]: "
    ));
    let selected_idx: usize = match choice.and_then(|s| s.parse::<usize>().ok()) {
        Some(n) if n >= 1 && n <= total => n - 1,
        _ => {
            println!("  Invalid selection.");
            return Ok(());
        }
    };

    let chosen = &catalog_rows[selected_idx];
    let model_alias = &chosen.alias;
    println!(
        "\n  Selected: \x1b[32m{}\x1b[0m ({})",
        model_alias, chosen.variant_id
    );

    // ── Download & load the model ────────────────────────────────────────

    let model = catalog.get_model(model_alias).await?;
    model.select_variant(&chosen.variant_id)?;

    ui::section(&format!("Model – {model_alias}"));

    if !model.is_cached().await? {
        ui::show_download_bar(model_alias);
        let alias_cb = model_alias.clone();
        model
            .download(Some(Box::new(move |progress: &str| {
                ui::update_download_bar(&alias_cb, progress);
            })))
            .await?;
        ui::finalize_download_bar(model_alias);
    }

    model.load().await?;
    println!("  \x1b[32m✓\x1b[0m Model loaded\n");

    // ── Detect task type ─────────────────────────────────────────────────

    let task_type = model
        .info()
        .task
        .as_deref()
        .unwrap_or("")
        .to_lowercase();
    let is_audio = task_type.contains("speech-recognition")
        || task_type.contains("speech-to-text")
        || model_alias.to_lowercase().contains("whisper");

    if is_audio {
        // ── Audio Transcription ──────────────────────────────────────────

        ui::section("Audio Transcription  (enter a file path, /quit to exit)");

        let audio_client = model.create_audio_client().language("en");

        loop {
            let input = ui::ask_user("  \x1b[36maudio file> \x1b[0m");
            let trimmed = match &input {
                Some(s) => s.trim(),
                None => break,
            };
            if trimmed.is_empty() {
                continue;
            }
            if matches!(trimmed, "/quit" | "/exit" | "/q") {
                break;
            }

            let audio_path = std::path::Path::new(trimmed)
                .canonicalize()
                .unwrap_or_else(|_| trimmed.into());
            let audio_path_str = audio_path.to_string_lossy().to_string();
            println!("  {}\n", audio_path.display());

            let mut box_ui = ui::StreamBox::new();
            let mut had_error = false;
            match audio_client.transcribe_streaming(&audio_path_str).await {
                Ok(mut stream) => {
                    while let Some(chunk) = stream.next().await {
                        match chunk {
                            Ok(c) => {
                                for ch in c.text.chars() {
                                    box_ui.write_char(ch);
                                }
                            }
                            Err(e) => {
                                had_error = true;
                                box_ui.finish();
                                println!("  \x1b[31mError: {e}\x1b[0m\n");
                                break;
                            }
                        }
                    }
                    if !had_error {
                        box_ui.finish();
                    }
                }
                Err(e) => {
                    box_ui.finish();
                    println!("  \x1b[31mError: {e}\x1b[0m\n");
                }
            }
        }
    } else {
        // ── Interactive Chat ─────────────────────────────────────────────

        ui::section("Chat  (type a message, /quit to exit)");

        let client = model.create_chat_client().temperature(0.7).max_tokens(512);

        let mut messages: Vec<ChatCompletionRequestMessage> = vec![
            ChatCompletionRequestSystemMessage::from("You are a helpful assistant.").into(),
        ];

        loop {
            let input = ui::ask_user("  \x1b[36m> \x1b[0m");
            let trimmed = match &input {
                Some(s) => s.trim().to_string(),
                None => break,
            };
            if trimmed.is_empty() {
                continue;
            }
            if matches!(trimmed.as_str(), "/quit" | "/exit" | "/q") {
                break;
            }

            print!("\x1b[1A\r\x1b[K");
            io::stdout().flush().ok();
            ui::print_user_msg(&trimmed);

            messages.push(ChatCompletionRequestUserMessage::from(trimmed.as_str()).into());

            let mut box_ui = ui::StreamBox::new();
            let mut response = String::new();

            let mut stream = client.complete_streaming_chat(&messages, None).await?;
            while let Some(chunk) = stream.next().await {
                let chunk = chunk?;
                if let Some(choice) = chunk.choices.first() {
                    if let Some(ref content) = choice.delta.content {
                        response.push_str(content);
                        for c in content.chars() {
                            box_ui.write_char(c);
                        }
                    }
                }
            }
            box_ui.finish();

            messages.push(
                ChatCompletionRequestAssistantMessage {
                    content: Some(response.into()),
                    ..Default::default()
                }
                .into(),
            );
        }
    }

    // ── Clean up ─────────────────────────────────────────────────────────

    model.unload().await?;
    println!("  Goodbye!");

    Ok(())
}
