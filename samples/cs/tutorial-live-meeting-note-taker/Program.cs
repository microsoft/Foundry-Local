// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using System.Text;
using Microsoft.Extensions.Logging;
using NAudio.Wave;
// </imports>

// <init>
CancellationToken ct = CancellationToken.None;

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

using var loggerFactory = LoggerFactory.Create(builder =>
{
    builder.SetMinimumLevel(
        Microsoft.Extensions.Logging.LogLevel.Information
    );
});
var logger = loggerFactory.CreateLogger<Program>();

// Initialize the singleton instance
await FoundryLocalManager.CreateAsync(config, logger);
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

var catalog = await mgr.GetCatalogAsync();
// </init>

// <microphone_setup>
// Set up microphone capture (16 kHz, 16-bit, mono PCM)
using var waveIn = new WaveInEvent
{
    WaveFormat = new WaveFormat(16000, 16, 1)
};
// </microphone_setup>

// <live_transcription>
// Load the speech-to-text model
var speechModel = await catalog.GetModelAsync("whisper-tiny")
    ?? throw new Exception("Speech model not found");

await speechModel.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading speech model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await speechModel.LoadAsync();
Console.WriteLine("Speech model loaded.");

// Create a live audio transcription session
var audioClient = await speechModel.GetAudioClientAsync();
await using var session = audioClient.CreateLiveTranscriptionSession();
session.Settings.Language = "en";

// Stream microphone audio into the session
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

// Consume the transcription stream and accumulate text
var transcriptionText = new StringBuilder();
Console.WriteLine("Transcription:");
try
{
    await foreach (var response in session.GetTranscriptionStream(cts.Token))
    {
        var text = response.Content[0].Text;
        if (response.IsFinal)
        {
            Console.WriteLine(text);
            transcriptionText.Append(text);
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

// Unload the speech model to free memory
await speechModel.UnloadAsync();
// </live_transcription>

// <summarization>
// Load the chat model for summarization
var chatModel = await catalog.GetModelAsync("qwen2.5-0.5b")
    ?? throw new Exception("Chat model not found");

await chatModel.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading chat model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await chatModel.LoadAsync();
Console.WriteLine("Chat model loaded.");

// Summarize the transcription into organized notes
var chatClient = await chatModel.GetChatClientAsync();
var messages = new List<ChatMessage>
{
    new ChatMessage
    {
        Role = "system",
        Content = "You are a note-taking assistant. Summarize " +
                  "the following transcription into organized, " +
                  "concise notes with bullet points."
    },
    new ChatMessage
    {
        Role = "user",
        Content = transcriptionText.ToString()
    }
};

var chatResponse = await chatClient.CompleteChatAsync(messages, ct);
var summary = chatResponse.Choices[0].Message.Content;
Console.WriteLine($"\nMeeting Notes:\n{summary}");

// Clean up
await chatModel.UnloadAsync();
Console.WriteLine("\nDone. Models unloaded.");
// </summarization>
// </complete_code>
