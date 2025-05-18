using Microsoft.AI.Foundry.Local;
using OpenAI;
using OpenAI.Chat;
using System.ClientModel;
using System.Diagnostics.Metrics;

public class TestApp
{
    public static async Task Main(string[] args)
    {
        var app = new TestApp(); // Create an instance of TestApp  

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing catalog integration...");
        await app.TestCatalog(); // Call the instance method

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing cache operations...");
        await app.TestCacheOperations(); // Call the instance method

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing OpenAI integration (from stopped service)...");
        var manager = new FoundryManager();
        if (manager != null)
        {
            await manager.StopServiceAsync();
        }
        await app.TestOpenAIIntegration("qwen2.5-0.5b");
        
        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing OpenAI integration (test again service is started)...");
        await app.TestOpenAIIntegration("qwen2.5-0.5b");

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing service operations");
        await app.TestService(); // Call the instance method

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing model (un)loading");
        await app.TestModelLoadUnload("qwen2.5-0.5b"); // Call the instance method

        Console.WriteLine(new string('=', 80)); // Separator for clarity
        Console.WriteLine("Testing downloading");
        await app.TestDownload("qwen2.5-0.5b"); // Call the instance method

        Console.WriteLine("Press any key to exit...");
        Console.ReadKey(true);
    }

    private async Task TestCacheOperations()
    {
        var manager = new FoundryManager();
        Console.WriteLine($"Model cache location at {await manager.GetCacheLocationAsync()}");
        // Print out models in the cache
        var models = await manager.ListCachedModelsAsync();
        Console.WriteLine($"Found {models.Count} models in the cache:");
        foreach (var m in models)
        {
            Console.WriteLine($"Model: {m.Alias} ({m.ModelId})");
        }
    }

    private async Task TestService()
    {
        var manager = new FoundryManager();
        await manager.StartServiceAsync();
        // Print out whether the service is running
        Console.WriteLine($"Service running (should be true): {manager.IsServiceRunning}");
        // Print out the service endpoint and API key
        Console.WriteLine($"Service Uri: {manager.ServiceUri}");
        Console.WriteLine($"Endpoint {manager.Endpoint}");
        Console.WriteLine($"ApiKey: {manager.ApiKey}");
        // stop the service
        await manager.StopServiceAsync();
        Console.WriteLine($"Service stopped");
        Console.WriteLine($"Service running (should be false): {manager.IsServiceRunning}");
    }

    private async Task TestCatalog()
    // First test catalog listing  
    {
        var manager = new FoundryManager();
        foreach (var m in await manager.ListCatalogModelsAsync())
        {
            Console.WriteLine($"Model: {m.Alias} ({m.ModelId})");
        }
    }

    private async Task TestOpenAIIntegration(string aliasOrModelId)
    {
        var manager = await FoundryManager.StartModelAsync(aliasOrModelId);

        var model = await manager.GetModelInfoAsync(aliasOrModelId);
        ApiKeyCredential key = new ApiKeyCredential(manager.ApiKey);
        OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
        {
            Endpoint = manager.Endpoint
        });

        var chatClient = client.GetChatClient(model?.ModelId);

        CollectionResult<StreamingChatCompletionUpdate> completionUpdates = chatClient.CompleteChatStreaming("Why is the sky blue'");

        Console.Write($"[ASSISTANT]: ");
        foreach (StreamingChatCompletionUpdate completionUpdate in completionUpdates)
        {
            if (completionUpdate.ContentUpdate.Count > 0)
            {
                Console.Write(completionUpdate.ContentUpdate[0].Text);
            }
        }
    }

    private async Task TestModelLoadUnload(string aliasOrModelId)
    {
        var manager = new FoundryManager();
        // Load a model
        var model = await manager.LoadModelAsync(aliasOrModelId);
        Console.WriteLine($"Loaded model: {model.Alias} ({model.ModelId})");
        // Unload the model
        await manager.UnloadModelAsync(aliasOrModelId);
        Console.WriteLine($"Unloaded model: {model.Alias} ({model.ModelId})");
    }

    private async Task TestDownload(string aliasOrModelId)
    {
        var manager = new FoundryManager();

        // Download a model
        var model = await manager.DownloadModelAsync(aliasOrModelId, force: true);

        // test that the model can be loaded
        Console.WriteLine($"Downloaded model: {model!.Alias} ({model.ModelId})");
    }
}
