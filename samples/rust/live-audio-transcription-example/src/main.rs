// Live Audio Transcription — Foundry Local Rust SDK Example
//
// Usage:
//   cargo run                  # Live microphone transcription (press ENTER to stop)
//   cargo run -- --synth       # Use synthetic 440Hz sine wave instead of microphone

use std::env;
use std::io::{self, Write};
use std::sync::Arc;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
use tokio_stream::StreamExt;

const ALIAS: &str = "nemotron";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let use_synth = env::args().any(|a| a == "--synth");

    println!("===========================================================");
    println!("   Foundry Local -- Live Audio Transcription Demo (Rust)");
    println!("===========================================================");
    println!();

    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    let model = manager.catalog().get_model(ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: &str| {
                print!("\r  {progress}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");

    let audio_client = model.create_audio_client();
    let session = Arc::new(audio_client.create_live_transcription_session());
    session.start(None).await?;
    println!("✓ Session started\n");

    let mut stream = session.get_transcription_stream().await?;
    let read_task = tokio::spawn(async move {
        while let Some(result) = stream.next().await {
            match result {
                Ok(r) => {
                    if let Some(content) = r.content.first() {
                        let text = &content.text;
                        if r.is_final {
                            println!();
                            println!("  [FINAL] {text}");
                        } else if !text.is_empty() {
                            print!("{text}");
                            io::stdout().flush().ok();
                        }
                    }
                }
                Err(e) => {
                    eprintln!("\n[ERROR] Stream error: {e}");
                    break;
                }
            }
        }
    });

    if use_synth {
        let pcm_data = generate_sine_wave_pcm(16000, 3, 440.0);
        let chunk_size = 16000 / 10 * 2;
        let chunk_interval = std::time::Duration::from_millis(100);
        for offset in (0..pcm_data.len()).step_by(chunk_size) {
            let end = std::cmp::min(offset + chunk_size, pcm_data.len());
            session.append(&pcm_data[offset..end], None).await?;
            tokio::time::sleep(chunk_interval).await;
        }
    } else {
        let host = cpal::default_host();
        let device = host.default_input_device().expect("No input audio device available");
        let default_config = device.default_input_config()?;
        let device_rate = default_config.sample_rate().0;
        let device_channels = default_config.channels();
        let mic_config: cpal::StreamConfig = default_config.into();

        let (audio_tx, mut audio_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(100);
        let input_stream = device.build_input_stream(
            &mic_config,
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                let bytes = convert_audio(data, device_channels, device_rate);
                if !bytes.is_empty() {
                    let _ = audio_tx.try_send(bytes);
                }
            },
            |err| eprintln!("Microphone stream error: {err}"),
            None,
        )?;

        input_stream.play()?;

        println!("Press ENTER to stop recording.");
        let session_for_forward = Arc::clone(&session);
        let forward_task = tokio::spawn(async move {
            while let Some(bytes) = audio_rx.recv().await {
                if let Err(e) = session_for_forward.append(&bytes, None).await {
                    eprintln!("Append error: {e}");
                    break;
                }
            }
        });

        let mut line = String::new();
        io::stdin().read_line(&mut line)?;
        drop(input_stream);
        forward_task.await?;
    }

    session.stop(None).await?;
    read_task.await?;

    model.unload().await?;
    Ok(())
}

fn convert_audio(data: &[f32], channels: u16, sample_rate: u32) -> Vec<u8> {
    let mono: Vec<f32> = if channels > 1 {
        data.chunks(channels as usize)
            .map(|frame| frame.iter().sum::<f32>() / channels as f32)
            .collect()
    } else {
        data.to_vec()
    };

    let resampled = if sample_rate != 16000 {
        resample(&mono, sample_rate, 16000)
    } else {
        mono
    };

    let mut bytes = Vec::with_capacity(resampled.len() * 2);
    for &s in &resampled {
        let clamped = s.clamp(-1.0, 1.0);
        let sample = (clamped * i16::MAX as f32) as i16;
        bytes.extend_from_slice(&sample.to_le_bytes());
    }
    bytes
}

fn generate_sine_wave_pcm(sample_rate: i32, duration_seconds: i32, frequency: f64) -> Vec<u8> {
    let total_samples = (sample_rate * duration_seconds) as usize;
    let mut pcm_bytes = vec![0u8; total_samples * 2];

    for i in 0..total_samples {
        let t = i as f64 / sample_rate as f64;
        let sample =
            (i16::MAX as f64 * 0.5 * (2.0 * std::f64::consts::PI * frequency * t).sin()) as i16;
        let bytes = sample.to_le_bytes();
        pcm_bytes[i * 2] = bytes[0];
        pcm_bytes[i * 2 + 1] = bytes[1];
    }

    pcm_bytes
}

fn resample(input: &[f32], from_rate: u32, to_rate: u32) -> Vec<f32> {
    if from_rate == to_rate {
        return input.to_vec();
    }

    let ratio = from_rate as f64 / to_rate as f64;
    let out_len = (input.len() as f64 / ratio).ceil() as usize;
    let mut output = Vec::with_capacity(out_len);

    for i in 0..out_len {
        let src_idx = i as f64 * ratio;
        let idx = src_idx as usize;
        let frac = src_idx - idx as f64;
        let s0 = input[idx.min(input.len() - 1)];
        let s1 = input[(idx + 1).min(input.len() - 1)];
        output.push(s0 + (s1 - s0) * frac as f32);
    }

    output
}
