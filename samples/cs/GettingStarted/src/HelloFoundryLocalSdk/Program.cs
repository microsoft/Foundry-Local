using Microsoft.AI.Foundry.Local;
using System.Linq;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

// Initialize the SDK
var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

// Discover and download execution providers
var eps = mgr.DiscoverEps();
Console.WriteLine($"Found {eps.Length} EP(s):");
if (eps.Length > 0)
{
    var maxNameLen = eps.Max(ep => ep.Name.Length);
    foreach (var ep in eps)
        Console.WriteLine($"  {ep.Name.PadRight(maxNameLen)}  (registered: {ep.IsRegistered})");

    string currentEp = "";
    await mgr.EnsureEpsDownloadedAsync(
        names: eps.Select(ep => ep.Name).ToArray(),
        progressCallback: (name, percent) =>
        {
            if (name != currentEp) { if (currentEp.Length > 0) Console.WriteLine(); currentEp = name; }
            Console.Write($"\r  {name.PadRight(maxNameLen)}  {percent:F1}%   ");
        });
    Console.WriteLine("\n✓ All EPs ready");
}
Console.WriteLine();

// Download and load a model
var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

await model.DownloadAsync(p => { Console.Write($"\rDownloading model: {p:F1}%"); });
Console.WriteLine();

await model.LoadAsync();
Console.WriteLine($"✓ Model {model.Id} loaded\n");

// Chat
var client = await model.GetChatClientAsync();
var messages = new List<ChatMessage> { new() { Role = "user", Content = "Why is the sky blue?" } };

Console.WriteLine("Response:");
await foreach (var chunk in client.CompleteChatStreamingAsync(messages, CancellationToken.None))
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine("\n");

await model.UnloadAsync();