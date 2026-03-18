// Live Audio Transcription — Foundry Local SDK Example
//
// Demonstrates real-time microphone-to-text using:
//   SDK (FoundryLocalManager) → Core (NativeAOT DLL) → onnxruntime-genai (StreamingProcessor)
//
// Prerequisites:
//   1. Nemotron ASR model downloaded to a local cache folder
//   2. Microsoft.AI.Foundry.Local.Core.dll (built from neutron-server with GenAI 0.13.0+)
//   3. onnxruntime-genai.dll + onnxruntime.dll + onnxruntime_providers_shared.dll (native GenAI)
//
// Usage:
//   dotnet run -- [model-cache-dir]
//   dotnet run -- C:\path\to\models

using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;
using NAudio.Wave;

// Parse model cache directory from args or use default
var modelCacheDir = args.Length > 0
    ? args[0]
    : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "FoundryLocal", "models");

var coreDllPath = Path.Combine(AppContext.BaseDirectory, "Microsoft.AI.Foundry.Local.Core.dll");

var loggerFactory = LoggerFactory.Create(b => b.AddConsole().SetMinimumLevel(Microsoft.Extensions.Logging.LogLevel.Information));
var logger = loggerFactory.CreateLogger("LiveAudioTranscription");

Console.WriteLine("===========================================================");
Console.WriteLine("   Foundry Local -- Live Audio Transcription Demo");
Console.WriteLine("===========================================================");
Console.WriteLine();
Console.WriteLine($"  Model cache: {modelCacheDir}");
Console.WriteLine($"  Core DLL:    {coreDllPath} (exists: {File.Exists(coreDllPath)})");
Console.WriteLine();

try
{
    // === Step 1: Initialize Foundry Local SDK ===
    Console.WriteLine("[1/5] Initializing Foundry Local SDK...");
    var config = new Configuration
    {
        AppName = "LiveAudioTranscription",
        LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
        ModelCacheDir = modelCacheDir,
        AdditionalSettings = new Dictionary<string, string>
        {
            { "FoundryLocalCorePath", coreDllPath }
        }
    };

    await FoundryLocalManager.CreateAsync(config, logger);
    Console.WriteLine("       SDK initialized.");

    // === Step 2: Find and load the nemotron ASR model ===
    Console.WriteLine("[2/5] Loading nemotron model...");
    var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();
    var model = await catalog.GetModelAsync("nemotron");

    if (model == null)
    {
        Console.WriteLine("ERROR: 'nemotron' not found in catalog.");
        Console.WriteLine($"       Ensure the model is downloaded to: {modelCacheDir}");
        Console.WriteLine("       The folder should contain genai_config.json, encoder.onnx, decoder.onnx, etc.");
        return;
    }

    Console.WriteLine($"       Found model: {model.Alias}");
    await model.LoadAsync();
    Console.WriteLine("       Model loaded.");

    // === Step 3: Create live transcription session ===
    Console.WriteLine("[3/5] Creating live transcription session...");
    var audioClient = await model.GetAudioClientAsync();
    var session = audioClient.CreateLiveTranscriptionSession();
    session.Settings.SampleRate = 16000;
    session.Settings.Channels = 1;
    session.Settings.Language = "en";

    await session.StartAsync();
    Console.WriteLine("       Session started (SDK -> Core -> GenAI pipeline active).");

    // === Step 4: Set up microphone + transcription reader ===
    Console.WriteLine("[4/5] Setting up microphone...");

    // Background task reads transcription results as they arrive
    var readTask = Task.Run(async () =>
    {
        try
        {
            await foreach (var result in session.GetTranscriptionStream())
            {
                if (result.IsFinal)
                {
                    Console.WriteLine();
                    Console.WriteLine($"  [FINAL] {result.Text}");
                    Console.Out.Flush();
                }
                else if (!string.IsNullOrEmpty(result.Text))
                {
                    Console.ForegroundColor = ConsoleColor.Cyan;
                    Console.Write(result.Text);
                    Console.ResetColor();
                    Console.Out.Flush();
                }
            }
        }
        catch (OperationCanceledException) { }
    });

    // Microphone capture via NAudio
    using var waveIn = new WaveInEvent
    {
        WaveFormat = new WaveFormat(rate: 16000, bits: 16, channels: 1),
        BufferMilliseconds = 100
    };

    int totalChunks = 0;
    long totalBytes = 0;

    waveIn.DataAvailable += (sender, e) =>
    {
        if (e.BytesRecorded > 0)
        {
            _ = session.AppendAsync(new ReadOnlyMemory<byte>(e.Buffer, 0, e.BytesRecorded));
            totalChunks++;
            totalBytes += e.BytesRecorded;
        }
    };

    // === Step 5: Record ===
    Console.WriteLine();
    Console.WriteLine("===========================================================");
    Console.WriteLine("  LIVE TRANSCRIPTION ACTIVE");
    Console.WriteLine("  Speak into your microphone.");
    Console.WriteLine("  Transcription appears in real-time (cyan text).");
    Console.WriteLine("  Press ENTER to stop recording.");
    Console.WriteLine("===========================================================");
    Console.WriteLine();

    waveIn.StartRecording();
    Console.ReadLine();
    waveIn.StopRecording();

    var totalSeconds = totalBytes / (16000.0 * 2);
    Console.WriteLine($"\n  Recording: {totalSeconds:F1}s | {totalChunks} chunks | {totalBytes / 1024} KB");

    // Stop session (flushes remaining audio through the pipeline)
    Console.WriteLine("\n[5/5] Stopping session...");
    await session.StopAsync();
    await readTask;

    // Unload model
    await model.UnloadAsync();

    Console.WriteLine();
    Console.WriteLine("===========================================================");
    Console.WriteLine("  Demo complete!");
    Console.WriteLine("  Pipeline: Mic -> NAudio -> SDK -> Core -> GenAI -> Text");
    Console.WriteLine("===========================================================");
}
catch (Exception ex)
{
    Console.WriteLine($"\nERROR: {ex.Message}");
    if (ex.InnerException != null)
        Console.WriteLine($"Inner: {ex.InnerException.Message}");
    Console.WriteLine($"\n{ex.StackTrace}");
}
