using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

CancellationToken ct = new CancellationToken();

// --- Optional private catalog configuration (from environment variables) ---
var mdsUri = Environment.GetEnvironmentVariable("MDS_URI");
var mdsClientId = Environment.GetEnvironmentVariable("MDS_CLIENT_ID");
var mdsClientSecret = Environment.GetEnvironmentVariable("MDS_CLIENT_SECRET");
var mdsTokenEndpoint = Environment.GetEnvironmentVariable("MDS_TOKEN_ENDPOINT");
var mdsAudience = Environment.GetEnvironmentVariable("MDS_AUDIENCE");

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize the singleton instance.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

// Ensure that any Execution Provider (EP) downloads run and are completed.
await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());

// Get the model catalog
var catalog = await mgr.GetCatalogAsync();

// === Private catalog flow (only runs if MDS_URI is set) ===
ModelVariant? model = null;

if (!string.IsNullOrEmpty(mdsUri))
{
    // STEP 1: Show catalogs before adding private catalog
    var catalogsBefore = await catalog.GetCatalogNamesAsync();
    Console.WriteLine($"Catalogs before: {string.Join(", ", catalogsBefore)}");

    // STEP 2: Register the private 3P catalog
    try
    {
        Console.WriteLine($"\nRegistering private catalog 'mds' at {mdsUri}...");
        await catalog.AddCatalogAsync(
            name: "mds",
            uri: new Uri(mdsUri),
            clientId: mdsClientId,
            clientSecret: mdsClientSecret,
            tokenEndpoint: mdsTokenEndpoint,
            audience: mdsAudience);
        Console.WriteLine("Private catalog registered.");

        // STEP 3: Verify the catalog was added
        var catalogsAfter = await catalog.GetCatalogNamesAsync();
        Console.WriteLine($"Catalogs after: {string.Join(", ", catalogsAfter)}");

        // STEP 4: List all models (public + private)
        Console.WriteLine("\n=== All Available Models (public + private) ===");
        var allModels = await catalog.ListModelsAsync();
        foreach (var m in allModels)
            foreach (var v in m.Variants)
                Console.WriteLine($"  - {v.Alias} ({v.Id})");

        // STEP 5: Filter to private catalog only
        Console.WriteLine("\n=== Private Catalog Models Only ===");
        await catalog.SelectCatalogAsync("mds");
        var privateModels = await catalog.ListModelsAsync();
        if (privateModels.Count == 0)
        {
            Console.WriteLine("  (no models in private catalog)");
        }
        else
        {
            foreach (var m in privateModels)
                foreach (var v in m.Variants)
                    Console.WriteLine($"  - {v.Alias} ({v.Id})");
        }

        // STEP 6: Pick a model from the private catalog
        if (privateModels.Count > 0)
        {
            var firstPrivate = privateModels[0].Variants[0];
            Console.WriteLine($"\nSelecting private catalog model: {firstPrivate.Id}");
            model = await catalog.GetModelVariantAsync(firstPrivate.Id);
        }

        // Reset to show all catalogs
        await catalog.SelectCatalogAsync(null);
    }
    catch (Exception ex)
    {
        Console.WriteLine($"\nWarning: Private catalog setup failed: {ex.Message}");
        Console.WriteLine("Falling back to public catalog.\n");
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
