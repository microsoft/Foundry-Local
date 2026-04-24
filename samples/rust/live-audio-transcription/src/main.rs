// NOTE: The live audio transcription API is not yet available in the Rust SDK.
// The live transcription API calls below are placeholders. [TO VERIFY]

// <complete_code>
// <imports>
use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
use std::io::{self, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use tokio_stream::StreamExt;
// </imports>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("live-audio-transcription"))?;

    manager
        .download_and_register_eps_with_progress(None, {
            |progress: f64| {
                print!("\rDownloading execution providers: {:.0}%", progress * 100.0);
                io::stdout().flush().ok();
            }
        })
        .await?;
    println!();

    let model = manager.catalog().get_model("nemotron-speech-streaming-en-0.6b").await?;
    if !model.is_cached().await? {
        model
            .download(Some(|progress: f64| {
                print!("\rDownloading model: {:.0}%", progress * 100.0);
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    model.load().await?;
    let audio_client = model.create_audio_client();
    // </init>

    // <live_transcription>
    // [TO VERIFY] - Live transcription API not yet available in Rust SDK
    let session = audio_client.create_live_transcription_session(); // [TO VERIFY]
    session.settings().set_language("en"); // [TO VERIFY]
    session.start().await?; // [TO VERIFY]

    // Set up microphone capture with cpal
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

    stream.play()?;
    println!("Listening... Press Ctrl+C to stop.\n");

    // Handle Ctrl+C for graceful shutdown
    let running = Arc::new(AtomicBool::new(true));
    let running_clone = running.clone();
    ctrlc::set_handler(move || {
        running_clone.store(false, Ordering::SeqCst);
    })?;

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
    while let Some(result) = stream_results.next().await {
        // [TO VERIFY] - Result structure may differ
        if let Some(content) = result.content.first() {
            if result.is_final {
                println!("{}", content.text);
            } else {
                print!("\r{}", content.text);
                io::stdout().flush().ok();
            }
        }

        if !running.load(Ordering::SeqCst) {
            break;
        }
    }

    // Clean up
    session.stop().await?; // [TO VERIFY]
    drop(stream); // Stop microphone capture
    model.unload().await?;
    // </live_transcription>

    println!("\nDone.");
    Ok(())
}
// </complete_code>
