// Live and file audio transcription — Foundry Local Rust SDK Example
//
// Usage:
//   cargo run                  # Live microphone (press Ctrl+C to stop)
//   cargo run -- --synth       # Synthetic 440Hz sine wave for live transcription
//   cargo run -- --file        # Transcribe bundled Recording.mp3 with Whisper
//   cargo run -- --file <path> # Transcribe a custom audio file with Whisper

use std::env;
use std::error::Error;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager, LiveAudioTranscriptionSession};
use tokio_stream::StreamExt;

const LIVE_ALIAS: &str = "nemotron-speech-streaming-en-0.6b";
// Multi-lingual (supports 30+ languages including auto-detect):
// const LIVE_ALIAS: &str = "nemotron-3.5-asr-streaming-0.6b";
const FILE_ALIAS: &str = "whisper-tiny";
const DEFAULT_AUDIO_FILE: &str = "Recording.mp3";

// Global flag for Ctrl+C graceful shutdown (mirrors JS process.on('SIGINT')).
static RUNNING: AtomicBool = AtomicBool::new(true);

enum RunMode {
    Help,
    Live { use_synth: bool },
    File { path: PathBuf },
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    match parse_args() {
        Ok(RunMode::Help) => {
            print_usage();
            Ok(())
        }
        Ok(RunMode::Live { use_synth }) => run_live_transcription(use_synth).await,
        Ok(RunMode::File { path }) => run_file_transcription(&path).await,
        Err(message) => {
            eprintln!("{message}\n");
            print_usage();
            std::process::exit(2);
        }
    }
}

fn parse_args() -> Result<RunMode, String> {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.iter().any(|arg| arg == "--help" || arg == "-h") {
        return Ok(RunMode::Help);
    }

    let mut use_synth = false;
    let mut file_path = None;
    let mut index = 0;

    while index < args.len() {
        match args[index].as_str() {
            "--synth" => {
                use_synth = true;
                index += 1;
            }
            "--file" => {
                if file_path.is_some() {
                    return Err("--file can only be specified once".to_string());
                }

                if let Some(next) = args.get(index + 1).filter(|arg| !arg.starts_with("--")) {
                    file_path = Some(PathBuf::from(next));
                    index += 2;
                } else {
                    file_path = Some(default_audio_path());
                    index += 1;
                }
            }
            other => {
                if let Some(path) = other.strip_prefix("--file=") {
                    if path.is_empty() {
                        return Err("--file= requires a path".to_string());
                    }
                    if file_path.is_some() {
                        return Err("--file can only be specified once".to_string());
                    }
                    file_path = Some(PathBuf::from(path));
                    index += 1;
                } else {
                    return Err(format!("Unknown argument: {other}"));
                }
            }
        }
    }

    if file_path.is_some() && use_synth {
        return Err("--synth cannot be combined with --file".to_string());
    }

    if let Some(path) = file_path {
        Ok(RunMode::File { path })
    } else {
        Ok(RunMode::Live { use_synth })
    }
}

fn print_usage() {
    eprintln!("Usage:");
    eprintln!("  cargo run -p audio");
    eprintln!("  cargo run -p audio -- --synth");
    eprintln!("  cargo run -p audio -- --file [audio_path]");
    eprintln!("  cargo run -p audio -- --file=audio_path");
}

fn default_audio_path() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(DEFAULT_AUDIO_FILE)
}

async fn run_live_transcription(use_synth: bool) -> Result<(), Box<dyn Error>> {
    let running = Arc::new(AtomicBool::new(true));
    let running_for_signal = running.clone();
    ctrlc::set_handler(move || {
        RUNNING.store(false, Ordering::SeqCst);
        running_for_signal.store(false, Ordering::SeqCst);
    })?;

    println!("===========================================================");
    println!("   Foundry Local -- Live Audio Transcription Demo (Rust)");
    println!("===========================================================");
    println!();

    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    download_execution_providers(manager).await?;

    let model = manager.catalog().get_model(LIVE_ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

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

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");

    let audio_client = model.create_audio_client();
    let mut session = audio_client.create_live_transcription_session();
    session.settings.language = Some("en".into());
    // session.settings.language = Some("de".into());
    // session.settings.language = Some("zh-CN".into());
    // session.settings.language = Some("auto".into());
    let session = Arc::new(session);
    session.start(None).await?;
    println!("✓ Session started\n");

    let mut stream = session.get_stream().await?;
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

    let mut mic_active = false;

    if !use_synth {
        match try_start_mic(&session, &running).await {
            Ok(()) => {
                mic_active = true;
            }
            Err(e) => {
                eprintln!("Could not initialize microphone: {e}");
                eprintln!("Falling back to synthetic audio test...\n");
            }
        }
    }

    if !mic_active {
        println!("Pushing synthetic audio (440Hz sine, 2s)...");
        let pcm_data = generate_sine_wave_pcm(16000, 2, 440.0);
        let chunk_size = 16000 / 10 * 2;
        let chunk_interval = std::time::Duration::from_millis(100);
        for offset in (0..pcm_data.len()).step_by(chunk_size) {
            if !running.load(Ordering::SeqCst) {
                break;
            }
            let end = std::cmp::min(offset + chunk_size, pcm_data.len());
            session.append(&pcm_data[offset..end], None).await?;
            tokio::time::sleep(chunk_interval).await;
        }
        println!("✓ Synthetic audio pushed");

        tokio::time::sleep(std::time::Duration::from_secs(3)).await;
    }

    println!("\n\nStopping...");
    session.stop(None).await?;
    read_task.await?;
    model.unload().await?;
    println!("✓ Done");
    Ok(())
}

async fn run_file_transcription(path: &Path) -> Result<(), Box<dyn Error>> {
    if !path.exists() {
        return Err(format!("Audio file not found: {}", path.display()).into());
    }

    println!("===========================================================");
    println!("   Foundry Local -- File Audio Transcription Demo (Rust)");
    println!("===========================================================");
    println!();
    println!("Audio file: {}", path.display());

    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    download_execution_providers(manager).await?;

    let model = manager.catalog().get_model(FILE_ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

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

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");

    let audio_client = model.create_audio_client();
    let audio_path = path.to_string_lossy();

    println!("--- Non-streaming transcription ---");
    let result = audio_client.transcribe(audio_path.as_ref()).await?;
    println!("Transcription: {}", result.text);

    println!("\n--- Streaming transcription ---");
    print!("Transcription: ");
    let mut stream = audio_client
        .transcribe_streaming(audio_path.as_ref())
        .await?;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        print!("{}", chunk.text);
        io::stdout().flush().ok();
    }
    println!("\n");

    println!("Unloading model...");
    model.unload().await?;
    println!("Done.");
    Ok(())
}

async fn download_execution_providers(manager: &FoundryLocalManager) -> Result<(), Box<dyn Error>> {
    manager
        .download_and_register_eps_with_progress(None, {
            let mut current_ep = String::new();
            move |ep_name: &str, percent: f64| {
                if ep_name != current_ep {
                    if !current_ep.is_empty() {
                        println!();
                    }
                    current_ep = ep_name.to_string();
                }
                print!("\r  {:<30}  {:5.1}%", ep_name, percent);
                io::stdout().flush().ok();
            }
        })
        .await?;
    println!();
    Ok(())
}

/// Try to open the default microphone with CPAL and forward PCM to the session.
/// Blocks until Ctrl+C is pressed.
async fn try_start_mic(
    session: &Arc<LiveAudioTranscriptionSession>,
    running: &Arc<AtomicBool>,
) -> Result<(), Box<dyn Error>> {
    let host = cpal::default_host();
    let device = host
        .default_input_device()
        .ok_or("No input audio device available")?;
    let default_config = device.default_input_config()?;
    let device_rate = default_config.sample_rate().0;
    let device_channels = default_config.channels();
    let sample_format = default_config.sample_format();

    let mic_config = cpal::StreamConfig {
        channels: device_channels,
        sample_rate: cpal::SampleRate(device_rate),
        buffer_size: cpal::BufferSize::Default,
    };

    let (audio_tx, mut audio_rx) = tokio::sync::mpsc::channel::<Vec<u8>>(100);
    let err_fn = |err| eprintln!("Microphone stream error: {err}");

    let input_stream = match sample_format {
        cpal::SampleFormat::F32 => {
            let tx = audio_tx.clone();
            device.build_input_stream(
                &mic_config,
                move |data: &[f32], _: &cpal::InputCallbackInfo| {
                    let bytes = convert_audio(data, device_channels, device_rate);
                    if !bytes.is_empty() {
                        let _ = tx.try_send(bytes);
                    }
                },
                err_fn,
                None,
            )?
        }
        cpal::SampleFormat::I16 => {
            let tx = audio_tx.clone();
            device.build_input_stream(
                &mic_config,
                move |data: &[i16], _: &cpal::InputCallbackInfo| {
                    let samples: Vec<f32> =
                        data.iter().map(|&s| s as f32 / i16::MAX as f32).collect();
                    let bytes = convert_audio(&samples, device_channels, device_rate);
                    if !bytes.is_empty() {
                        let _ = tx.try_send(bytes);
                    }
                },
                err_fn,
                None,
            )?
        }
        cpal::SampleFormat::U16 => {
            let tx = audio_tx.clone();
            device.build_input_stream(
                &mic_config,
                move |data: &[u16], _: &cpal::InputCallbackInfo| {
                    let samples: Vec<f32> = data
                        .iter()
                        .map(|&s| (s as f32 / u16::MAX as f32) * 2.0 - 1.0)
                        .collect();
                    let bytes = convert_audio(&samples, device_channels, device_rate);
                    if !bytes.is_empty() {
                        let _ = tx.try_send(bytes);
                    }
                },
                err_fn,
                None,
            )?
        }
        other => {
            return Err(format!("Unsupported input sample format: {other:?}").into());
        }
    };
    drop(audio_tx);

    input_stream.play()?;

    println!("===========================================================");
    println!("  LIVE TRANSCRIPTION ACTIVE");
    println!("  Speak into your microphone.");
    println!("  Press Ctrl+C to stop.");
    println!("===========================================================");
    println!();

    let session_clone = Arc::clone(session);
    let forward_task = tokio::spawn(async move {
        while let Some(bytes) = audio_rx.recv().await {
            if let Err(e) = session_clone.append(&bytes, None).await {
                eprintln!("Append error: {e}");
                break;
            }
        }
    });

    while running.load(Ordering::SeqCst) {
        tokio::time::sleep(std::time::Duration::from_millis(100)).await;
    }

    drop(input_stream);
    forward_task.await?;
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
    if from_rate == to_rate || input.is_empty() {
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
