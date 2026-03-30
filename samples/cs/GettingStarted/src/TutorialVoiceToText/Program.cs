// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Microsoft.Extensions.Logging;
using System.Text;
// </imports>

// <init>
CancellationToken ct = CancellationToken.None;

var config = new Configuration
{
    AppName = "note-taker",
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
var catalog = await mgr.GetCatalogAsync();
// </init>

// <transcription>
// Load the speech-to-text model
var speechModel = await catalog.GetModelAsync("whisper")
    ?? throw new Exception("Speech model not found");

await speechModel.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading speech model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await speechModel.LoadAsync();
Console.WriteLine("Speech model loaded.");

// Transcribe the audio file
var audioClient = await speechModel.GetAudioClientAsync();
var transcriptionText = new StringBuilder();

Console.WriteLine("\nTranscription:");
var audioResponse = audioClient
    .TranscribeAudioStreamingAsync("meeting-notes.wav", ct);
await foreach (var chunk in audioResponse)
{
    Console.Write(chunk.Text);
    transcriptionText.Append(chunk.Text);
}
Console.WriteLine();

// Unload the speech model to free memory
await speechModel.UnloadAsync();
// </transcription>

// <summarization>
// Load the chat model for summarization
var chatModel = await catalog.GetModelAsync("phi-3.5-mini")
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
Console.WriteLine($"\nSummary:\n{summary}");

// Clean up
await chatModel.UnloadAsync();
Console.WriteLine("\nDone. Models unloaded.");
// </summarization>
// </complete_code>
