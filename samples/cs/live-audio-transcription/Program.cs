// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
using NAudio.Wave;
// </imports>

// <init>
var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize the singleton instance
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

// Download and register all execution providers
var currentEp = "";
await mgr.DownloadAndRegisterEpsAsync((epName, percent) =>
{
    if (epName != currentEp)
    {
        if (currentEp != "") Console.WriteLine();
        currentEp = epName;
    }
    Console.Write($"\r  {epName.PadRight(30)}  {percent,6:F1}%");
});
if (currentEp != "") Console.WriteLine();

// Download and load the whisper model
var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync("whisper-tiny")
    ?? throw new Exception("Model not found");

await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await model.LoadAsync();
Console.WriteLine("Whisper model loaded and ready.");
// </init>

// <live_transcription>
// Create a live audio transcription session
var audioClient = await model.GetAudioClientAsync();
await using var session = audioClient.CreateLiveTranscriptionSession();
session.Settings.Language = "en";

// Set up microphone capture (16 kHz, 16-bit, mono PCM)
using var waveIn = new WaveInEvent
{
    WaveFormat = new WaveFormat(16000, 16, 1)
};

waveIn.DataAvailable += async (sender, e) =>
{
    if (e.BytesRecorded > 0)
    {
        var buffer = new byte[e.BytesRecorded];
        Array.Copy(e.Buffer, buffer, e.BytesRecorded);
        await session.AppendAsync(new ReadOnlyMemory<byte>(buffer));
    }
};

// Handle Ctrl+C to stop recording gracefully
using var cts = new CancellationTokenSource();
Console.CancelKeyPress += (sender, e) =>
{
    e.Cancel = true;
    cts.Cancel();
};

// Start the session and begin recording
await session.StartAsync();
waveIn.StartRecording();
Console.WriteLine("Listening... Press Ctrl+C to stop.\n");

// Consume the transcription stream
try
{
    await foreach (var response in session.GetTranscriptionStream(cts.Token))
    {
        var text = response.Content[0].Text;
        if (response.IsFinal)
        {
            Console.WriteLine(text);
        }
        else
        {
            Console.Write($"\r{text}");
        }
    }
}
catch (OperationCanceledException)
{
    // Expected when Ctrl+C is pressed
}

// Stop recording and drain remaining audio
waveIn.StopRecording();
await session.StopAsync();
Console.WriteLine("\nRecording stopped.");
// </live_transcription>

// Clean up - unload the model
await model.UnloadAsync();
Console.WriteLine("Model unloaded. Goodbye!");
// </complete_code>
