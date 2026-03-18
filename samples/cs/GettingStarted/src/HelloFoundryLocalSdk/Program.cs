using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize the singleton instance.
// If PrivateCatalogUri is set in appsettings.json (or MDS_URI env var),
// the private catalog is auto-connected during initialization.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

// Ensure that any Execution Provider (EP) downloads run and are completed.
await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());

// Get the model catalog
var catalog = await mgr.GetCatalogAsync();

// Show available catalogs (includes auto-connected private catalog if configured)
var catalogNames = await catalog.GetCatalogNamesAsync();
Console.WriteLine($"Available catalogs: {string.Join(", ", catalogNames)}");

// List all models (public + private)
Console.WriteLine("\n=== All Available Models ===");
var allModels = await catalog.ListModelsAsync();
foreach (var m in allModels)
    foreach (var v in m.Variants)
        Console.WriteLine($"  - {v.Alias} ({v.Id})");

// Try to pick a private catalog model first, then fall back to public
ModelVariant? model = null;

if (catalogNames.Contains("private"))
{
    Console.WriteLine("\n=== Private Catalog Models ===");
    await catalog.SelectCatalogAsync("private");
    var privateModels = await catalog.ListModelsAsync();
    foreach (var m in privateModels)
        foreach (var v in m.Variants)
            Console.WriteLine($"  - {v.Alias} ({v.Id})");

    if (privateModels.Count > 0)
    {
        var firstPrivate = privateModels[0].Variants[0];
        Console.WriteLine($"\nSelecting private catalog model: {firstPrivate.Id}");
        model = await catalog.GetModelVariantAsync(firstPrivate.Id);
    }

    // Reset to show all catalogs
    await catalog.SelectCatalogAsync(null);
}

// Fallback to a public model if no private model was selected
if (model == null)
{
    var fallbackModel = (await catalog.ListModelsAsync())
        .SelectMany(m => m.Variants)
        .FirstOrDefault(v => v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase));
    if (fallbackModel != null)
    {
        Console.WriteLine($"\nUsing public model: {fallbackModel.Id}");
        model = fallbackModel;
    }
    else
    {
        throw new Exception("No compatible models found in any catalog.");
    }
}

// Download the model (skips if already cached)
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
