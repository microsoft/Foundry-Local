using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};


// Initialize the singleton instance.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;


// Ensure that any Execution Provider (EP) downloads run and are completed.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
// For cross platform builds there is no dynamic EP download and this will return immediately.
await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());


// Get the model catalog
var catalog = await mgr.GetCatalogAsync();


// Get a model using an alias.
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

// Check cache before downloading — skip download if model is already cached
if (!await model.IsCachedAsync())
{
    Console.WriteLine($"Model \"{model.Id}\" not found in cache. Downloading...");
    await model.DownloadAsync(progress =>
    {
        var filled = (int)Math.Round(progress / 100.0 * 30);
        var bar = new string('\u2588', filled) + new string('\u2591', 30 - filled);
        Console.Write($"\rDownloading: [{bar}] {progress:F1}%");
        if (progress >= 100f)
        {
            Console.WriteLine();
        }
    });
    Console.WriteLine("\u2713 Model downloaded");
}
else
{
    Console.WriteLine($"\u2713 Model \"{model.Id}\" already cached \u2014 skipping download");
}

// Load the model into memory
Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done. \u2713 Model ready");

// Get a chat client
var chatClient = await model.GetChatClientAsync();

// Create a chat message
List<ChatMessage> messages = new()
{
    new ChatMessage { Role = "user", Content = "Why is the sky blue?" }
};

// Get a streaming chat completion response
Console.WriteLine("Chat completion response:");
var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in streamingResponse)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

// Tidy up - unload the model
await model.UnloadAsync();