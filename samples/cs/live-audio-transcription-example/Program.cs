// Live Audio Transcription — Foundry Local SDK Example
//
// Demonstrates real-time microphone-to-text using:
//   SDK (FoundryLocalManager) → Core (NativeAOT DLL) → onnxruntime-genai (StreamingProcessor)

using Microsoft.AI.Foundry.Local;
using NAudio.Wave;

Console.WriteLine("===========================================================");
Console.WriteLine("   Foundry Local -- Live Audio Transcription Demo");
Console.WriteLine("===========================================================");
Console.WriteLine();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

await Utils.RunWithSpinner("Registering execution providers", mgr.DownloadAndRegisterEpsAsync());

var catalog = await mgr.GetCatalogAsync();

var model = await catalog.GetModelAsync("nemotron-speech-streaming-en-0.6b") ?? throw new Exception("Model \"nemotron-speech-streaming-en-0.6b\" not found in catalog");

await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f)
    {
        Console.WriteLine();
    }
});

Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");

var audioClient = await model.GetAudioClientAsync();
var session = audioClient.CreateLiveTranscriptionSession();
session.Settings.SampleRate = 16000;  // Default is 16000; shown here to match the NAudio WaveFormat below
session.Settings.Channels = 1;
session.Settings.Language = "en";

await session.StartAsync();
Console.WriteLine("       Session started");

var readTask = Task.Run(async () =>
{
    try
    {
        await foreach (var result in session.GetTranscriptionStream())
        {
            var text = result.Content?[0]?.Text;
            if (result.IsFinal)
            {
                Console.WriteLine();
                Console.WriteLine($"  [FINAL] {text}");
                Console.Out.Flush();
            }
            else if (!string.IsNullOrEmpty(text))
            {
                Console.ForegroundColor = ConsoleColor.Cyan;
                Console.Write(text);
                Console.ResetColor();
                Console.Out.Flush();
            }
        }
    }
    catch (OperationCanceledException) { }
});

using var waveIn = new WaveInEvent
{
    WaveFormat = new WaveFormat(rate: 16000, bits: 16, channels: 1),
    BufferMilliseconds = 100
};

// Use a bounded channel to avoid unbounded fire-and-forget AppendAsync calls.
// NAudio's DataAvailable callback is synchronous, so we enqueue PCM chunks and
// await AppendAsync on a dedicated task to respect SDK backpressure.
var audioChannel = System.Threading.Channels.Channel.CreateBounded<byte[]>(
    new System.Threading.Channels.BoundedChannelOptions(50)
    {
        FullMode = System.Threading.Channels.BoundedChannelFullMode.DropOldest
    });

var appendTask = Task.Run(async () =>
{
    await foreach (var chunk in audioChannel.Reader.ReadAllAsync())
    {
        await session.AppendAsync(chunk);
    }
});

waveIn.DataAvailable += (sender, e) =>
{
    if (e.BytesRecorded > 0)
    {
        var buffer = new byte[e.BytesRecorded];
        Buffer.BlockCopy(e.Buffer, 0, buffer, 0, e.BytesRecorded);
        audioChannel.Writer.TryWrite(buffer);
    }
};

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

audioChannel.Writer.Complete();
await appendTask;

await session.StopAsync();
await readTask;

await model.UnloadAsync();
