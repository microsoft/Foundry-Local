using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;

var loggerFactory = LoggerFactory.Create(b => b.AddConsole().SetMinimumLevel(LogLevel.Debug));
var logger = loggerFactory.CreateLogger("AudioStreamTest");

// Point to the directory containing Core + ORT DLLs
var corePath = @"C:\Users\ruiren\Desktop\audio-stream-test\Microsoft.AI.Foundry.Local.Core.dll";

var config = new Configuration
{
    AppName = "AudioStreamTest",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Debug,
    AdditionalSettings = new Dictionary<string, string>
    {
        { "FoundryLocalCorePath", corePath }
    }
};

Console.WriteLine("=== Initializing FoundryLocalManager ===");
await FoundryLocalManager.CreateAsync(config, logger);
var manager = FoundryLocalManager.Instance;

Console.WriteLine("=== Getting Catalog ===");
var catalog = await manager.GetCatalogAsync();
var models = await catalog.ListModelsAsync();
Console.WriteLine($"Found {models.Count} models");

// Find and load a whisper model
var model = await catalog.GetModelAsync("whisper-tiny");
if (model == null)
{
    Console.WriteLine("whisper-tiny not found. Available models:");
    foreach (var m in models)
        Console.WriteLine($"  - {m.Alias}");
    return;
}

Console.WriteLine($"=== Downloading {model.Alias} ===");
await model.DownloadAsync(p => Console.Write($"\r  Progress: {p:F1}%"));
Console.WriteLine();

Console.WriteLine($"=== Loading {model.Alias} ===");
await model.LoadAsync();
Console.WriteLine("Model loaded.");

Console.WriteLine("=== Creating streaming session ===");
var audioClient = await model.GetAudioClientAsync();
var streamingClient = audioClient.CreateStreamingSession();
streamingClient.Settings.SampleRate = 16000;
streamingClient.Settings.Channels = 1;
streamingClient.Settings.BitsPerSample = 16;
streamingClient.Settings.Language = "en";

Console.WriteLine("=== Starting streaming session ===");
await streamingClient.StartAsync();
Console.WriteLine("Session started!");

// Push some fake PCM data (silence — 100ms at 16kHz 16-bit mono = 3200 bytes)
var fakePcm = new byte[3200];
Console.WriteLine("=== Pushing audio chunks ===");
for (int i = 0; i < 5; i++)
{
    await streamingClient.AppendAsync(fakePcm);
    Console.WriteLine($"  Pushed chunk {i + 1}");
}

Console.WriteLine("=== Stopping session ===");
await streamingClient.StopAsync();
Console.WriteLine("Session stopped.");

Console.WriteLine("=== Unloading model ===");
await model.UnloadAsync();
Console.WriteLine("Done! All plumbing works end-to-end.");