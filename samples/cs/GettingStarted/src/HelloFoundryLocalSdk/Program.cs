using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize — auto-connects private catalog from appsettings.json if configured
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());

var catalog = await mgr.GetCatalogAsync();

// Show available catalogs
var catalogNames = await catalog.GetCatalogNamesAsync();
Console.WriteLine($"\nCatalogs: {string.Join(", ", catalogNames)}");
bool hasPrivate = catalogNames.Any(n => n == "private");

// List all models (public + private)
Console.WriteLine("\n=== All Available Models (public + private) ===");
var allModels = await catalog.ListModelsAsync();
foreach (var m in allModels)
    foreach (var v in m.Variants)
        Console.WriteLine($"  - {v.Alias} ({v.Id})");

// Show private catalog models if available
ModelVariant? model = null;

if (hasPrivate)
{
    Console.WriteLine("\n=== Private Catalog Models ===");
    try
    {
        await catalog.SelectCatalogAsync("private");
        var privateModels = await catalog.ListModelsAsync();
        await catalog.SelectCatalogAsync(null);

        if (privateModels.Count > 0)
        {
            foreach (var m in privateModels)
                foreach (var v in m.Variants)
                    Console.WriteLine($"  - {v.Alias} ({v.Id})");

            var firstPrivate = privateModels[0].Variants[0];
            Console.WriteLine($"\nSelecting private model: {firstPrivate.Id}");
            model = await catalog.GetModelVariantAsync(firstPrivate.Id);
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"  (filter failed: {ex.Message})");
        // Fallback: find by MDS_MODEL env var from combined list
        var target = Environment.GetEnvironmentVariable("MDS_MODEL") ?? "";
        if (!string.IsNullOrEmpty(target))
        {
            var match = allModels.SelectMany(m => m.Variants)
                .FirstOrDefault(v => v.Id.Contains(target, StringComparison.OrdinalIgnoreCase));
            if (match != null) { model = match; Console.WriteLine($"  Found: {match.Id}"); }
        }
    }
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
