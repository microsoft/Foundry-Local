// NOTE: The live audio transcription API is not yet available in the Rust SDK.
// The live transcription API calls below are placeholders. [TO VERIFY]

// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage, FoundryLocalConfig, FoundryLocalManager,
};
use std::io::{self, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use tokio_stream::StreamExt;
// </imports>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    let manager =
        FoundryLocalManager::create(FoundryLocalConfig::new("tutorial-live-meeting-note-taker"))?;

    manager
        .download_and_register_eps_with_progress(None, {
            |progress: f64| {
                print!("\rDownloading execution providers: {:.0}%", progress * 100.0);
                io::stdout().flush().ok();
            }
        })
        .await?;
    println!();

    let whisper_model = manager.catalog().get_model("whisper-tiny").await?;
    if !whisper_model.is_cached().await? {
        whisper_model
            .download(Some(|progress: f64| {
                print!("\rDownloading whisper model: {:.0}%", progress * 100.0);
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    whisper_model.load().await?;
    let audio_client = whisper_model.create_audio_client();
    // </init>

    // <microphone_setup>
    let host = cpal::default_host();
    let device = host
        .default_input_device()
        .expect("No input device available");

    let config = cpal::StreamConfig {
        channels: 1,
        sample_rate: cpal::SampleRate(16000),
        buffer_size: cpal::BufferSize::Default,
    };

    let (tx, rx) = std::sync::mpsc::channel::<Vec<u8>>();

    let stream = device.build_input_stream(
        &config,
        move |data: &[i16], _: &cpal::InputCallbackInfo| {
            let bytes: Vec<u8> = data.iter().flat_map(|s| s.to_le_bytes()).collect();
            let _ = tx.send(bytes);
        },
        |err| eprintln!("Audio input error: {}", err),
        None,
    )?;
    // </microphone_setup>

    // <live_transcription>
    // [TO VERIFY] - Live transcription API not yet available in Rust SDK
    let session = audio_client.create_live_transcription_session(); // [TO VERIFY]
    session.settings().set_language("en"); // [TO VERIFY]
    session.start().await?; // [TO VERIFY]

    stream.play()?;
    println!("Recording meeting... Press Ctrl+C to stop and generate notes.\n");

    // Handle Ctrl+C for graceful shutdown
    let running = Arc::new(AtomicBool::new(true));
    let running_clone = running.clone();
    ctrlc::set_handler(move || {
        running_clone.store(false, Ordering::SeqCst);
    })?;

    // Accumulate transcribed text for summarization
    let accumulated_text = Arc::new(Mutex::new(String::new()));

    // Spawn a task to feed microphone audio into the live transcription session
    let running_audio = running.clone();
    let session_clone = session.clone(); // [TO VERIFY]
    tokio::spawn(async move {
        while running_audio.load(Ordering::SeqCst) {
            if let Ok(pcm_data) = rx.recv() {
                // [TO VERIFY] - append method signature may differ
                if let Err(e) = session_clone.append(&pcm_data).await {
                    eprintln!("Error sending audio: {}", e);
                    break;
                }
            }
        }
    });

    // Consume transcription results as they arrive
    // [TO VERIFY] - Stream API may differ
    let mut stream_results = session.get_transcription_stream(); // [TO VERIFY]
    let accumulated_clone = accumulated_text.clone();
    while let Some(result) = stream_results.next().await {
        // [TO VERIFY] - Result structure may differ
        if let Some(content) = result.content.first() {
            if result.is_final {
                println!("{}", content.text);
                if let Ok(mut text) = accumulated_clone.lock() {
                    text.push_str(&content.text);
                    text.push(' ');
                }
            } else {
                print!("\r{}", content.text);
                io::stdout().flush().ok();
            }
        }

        if !running.load(Ordering::SeqCst) {
            break;
        }
    }

    // Clean up transcription resources
    session.stop().await?; // [TO VERIFY]
    drop(stream); // Stop microphone capture
    whisper_model.unload().await?;
    // </live_transcription>

    // <summarization>
    let accumulated_text = accumulated_text
        .lock()
        .map_err(|e| anyhow::anyhow!("Lock error: {}", e))?
        .clone();

    if accumulated_text.trim().is_empty() {
        println!("\nNo transcription captured. Skipping summarization.");
        return Ok(());
    }

    println!("\n--- Generating meeting notes ---\n");

    let chat_model = manager.catalog().get_model("qwen2.5-0.5b").await?;
    if !chat_model.is_cached().await? {
        chat_model
            .download(Some(|progress: f64| {
                print!("\rDownloading chat model: {:.0}%", progress * 100.0);
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }
    chat_model.load().await?;

    let client = chat_model
        .create_chat_client()
        .temperature(0.7)
        .max_tokens(512);

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(
            "You are a note-taking assistant. Summarize the following meeting transcript into \
             clear, organized meeting notes with key discussion points, decisions made, and \
             action items.",
        )
        .into(),
        ChatCompletionRequestUserMessage::from(accumulated_text.as_str()).into(),
    ];

    let response = client.complete_chat(&messages, None).await?;
    let summary = response.choices[0]
        .message
        .content
        .as_deref()
        .unwrap_or("");

    println!("Meeting Notes:\n{}", summary);

    chat_model.unload().await?;
    // </summarization>

    println!("\nDone.");
    Ok(())
}
// </complete_code>
