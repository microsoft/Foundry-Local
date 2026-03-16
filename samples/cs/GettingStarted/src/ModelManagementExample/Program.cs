using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize — auto-connects private catalog if MDS_URI env var (or config) is set.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());

var catalog = await mgr.GetCatalogAsync();

// Show connected catalogs
var catalogNames = await catalog.GetCatalogNamesAsync();
Console.WriteLine($"Connected catalogs: {string.Join(", ", catalogNames)}");

// List all models (public + private if credentials are present)
Console.WriteLine("\n=== All Models ===");
var models = await catalog.ListModelsAsync();
foreach (var m in models)
    foreach (var v in m.Variants)
        Console.WriteLine($"  - {v.Alias} ({v.Id})");

// Filter to private only (only if private catalog is connected)
if (catalogNames.Count() > 1)
{
    await catalog.SelectCatalogAsync("private");
    Console.WriteLine("\n=== Private Models Only ===");
    foreach (var m in await catalog.ListModelsAsync())
        foreach (var v in m.Variants)
            Console.WriteLine($"  - {v.Alias} ({v.Id})");

    // Filter to public only
    await catalog.SelectCatalogAsync("public");
    Console.WriteLine("\n=== Public Models Only ===");
    foreach (var m in await catalog.ListModelsAsync())
        foreach (var v in m.Variants)
            Console.WriteLine($"  - {v.Alias} ({v.Id})");

    // Reset to show all
    await catalog.SelectCatalogAsync(null);
}

// Pick a model, download, load, chat
var modelAlias = "qwen3-0.6b-generic-cpu";
var model = await catalog.GetModelAsync(modelAlias)
    ?? throw new Exception($"Model '{modelAlias}' not found");

Console.WriteLine($"\nSelected model: {model.Id}");

var cpuVariant = model.Variants.FirstOrDefault(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
if (cpuVariant != null) model.SelectVariant(cpuVariant);

// Download (skips if already cached)
await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading: {progress:F1}%");
    if (progress >= 100f) Console.WriteLine();
});

// Load into memory
await model.LoadAsync();
Console.WriteLine("Model loaded.");

// Chat
var chatClient = await model.GetChatClientAsync();
chatClient.Settings.Temperature = 0.7f;
chatClient.Settings.MaxTokens = 512;

List<ChatMessage> messages = [new() { Role = "user", Content = "Why is the sky blue?" }];

Console.WriteLine("\nChat response:");
var stream = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in stream)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

// Cleanup
await model.UnloadAsync();
Console.WriteLine("\nModel unloaded. Done.");
