using Microsoft.AI.Foundry.Local;
using System.Linq;
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


// Discover available execution providers
var eps = mgr.DiscoverEps();
Console.WriteLine($"Found {eps.Length} execution provider(s):");
var maxNameLen = eps.Max(ep => ep.Name.Length);
foreach (var ep in eps)
{
    Console.WriteLine($"  {ep.Name.PadRight(maxNameLen)}  (registered: {ep.IsRegistered})");
}

// Download and register all discovered EPs with per-EP progress
var epNames = eps.Select(ep => ep.Name).ToArray();
Console.WriteLine($"\nDownloading {epNames.Length} execution provider(s)...");
string? currentEp = null;
await mgr.EnsureEpsDownloadedAsync(
    names: epNames,
    progressCallback: (name, percent) =>
    {
        if (name != currentEp)
        {
            if (currentEp != null)
            {
                Console.WriteLine();
            }
            currentEp = name;
        }
        Console.Write($"\r  {name.PadRight(maxNameLen)}  {percent:F1}%   ");
    });
Console.WriteLine("\n✓ All execution providers ready");


// Get the model catalog
var catalog = await mgr.GetCatalogAsync();


// Get a model using an alias.
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

// Download the model (the method skips download if already cached)
await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f)
    {
        Console.WriteLine();
    }
});

// Load the model
Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");

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