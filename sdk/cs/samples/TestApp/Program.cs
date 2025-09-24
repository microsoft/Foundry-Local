// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

using System.ClientModel;

using Microsoft.AI.Foundry.Local;

using OpenAI;
using OpenAI.Chat;

public class TestApp
{
    public static async Task Main(string[] args)
    {
        var app = new TestApp(); // Create an instance of TestApp

        Console.WriteLine(new string('=', 80));
        Console.WriteLine("Testing catalog integration...");
        await app.TestCatalog(); // Call the instance method

        Console.WriteLine(new string('=', 80));
        Console.WriteLine("Testing cache operations...");
        await app.TestCacheOperations(); // Call the instance method

        string[] aliasesOrModelIds = new[] { "qwen2.5-0.5b", "qwen2.5-0.5b-instruct-generic-cpu:3" };

        foreach (var aliasOrModelId in aliasesOrModelIds)
        {
            Console.WriteLine(new string('=', 80));
            // don't stop for now. the catalog api doesn't return register EP models afer stop, start and model list
            // Console.WriteLine($"Testing OpenAI integration (from stopped service) with {aliasOrModelId}...");
            // using var manager = new FoundryLocalManager();
            // await manager.StopServiceAsync();
            // await app.TestOpenAIIntegration(aliasOrModelId);

            Console.WriteLine(new string('=', 80));
            Console.WriteLine($"Testing OpenAI integration (service running) with {aliasOrModelId}...");
            await app.TestOpenAIIntegration(aliasOrModelId);

            Console.WriteLine(new string('=', 80));
            Console.WriteLine($"Testing service operations with {aliasOrModelId}...");
            await app.TestService();

            Console.WriteLine(new string('=', 80));
            Console.WriteLine($"Testing model (un)loading with {aliasOrModelId}...");
            await app.TestModelLoadUnload(aliasOrModelId);

            // Console.WriteLine(new string('=', 80));
            // Console.WriteLine($"Testing force downloading with {aliasOrModelId}...");
            // await app.TestDownload(aliasOrModelId);
        }
    }

    private async Task TestCacheOperations()
    {
        using var manager = new FoundryLocalManager();
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
        using var manager = new FoundryLocalManager();
        await manager.StartServiceAsync();
        // Print out whether the service is running
        Console.WriteLine($"Service running (should be true): {manager.IsServiceRunning}");
        // Print out the service endpoint and API key
        Console.WriteLine($"Service Uri: {manager.ServiceUri}");
        Console.WriteLine($"Endpoint {manager.Endpoint}");
        Console.WriteLine($"ApiKey: {manager.ApiKey}");
        // don't stop for now. the catalog api doesn't return register EP models afer stop, start and model list
        // // stop the service
        // await manager.StopServiceAsync();
        // Console.WriteLine($"Service stopped");
        // Console.WriteLine($"Service running (should be false): {manager.IsServiceRunning}");
    }

    private async Task TestCatalog()
    // First test catalog listing
    {
        using var manager = new FoundryLocalManager();
        foreach (var m in await manager.ListCatalogModelsAsync())
        {
            Console.WriteLine($"Model: {m.Alias} ({m.ModelId})");
        }
    }

    private async Task TestOpenAIIntegration(string aliasOrModelId)
    {
        var manager = await FoundryLocalManager.StartModelAsync(aliasOrModelId);

        var model = await manager.GetModelInfoAsync(aliasOrModelId);
        var key = new ApiKeyCredential(manager.ApiKey);
        var client = new OpenAIClient(key, new OpenAIClientOptions
        {
            Endpoint = manager.Endpoint
        });

        var chatClient = client.GetChatClient(model?.ModelId);

        CollectionResult<StreamingChatCompletionUpdate> completionUpdates = chatClient.CompleteChatStreaming("Why is the sky blue?");

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
        using var manager = new FoundryLocalManager();

        // Load the model
        var model = await manager.LoadModelAsync(aliasOrModelId);
        Console.WriteLine($"Loaded model: {model.Alias} ({model.ModelId})");

        // Attempt to unload without forcing
        await manager.UnloadModelAsync(aliasOrModelId);
        var stillLoaded = (await manager.ListLoadedModelsAsync())
            .Any(m => m.ModelId == model.ModelId);
        Console.WriteLine($"Model still loaded (expected: True): {stillLoaded}");

        // Force unload
        await manager.UnloadModelAsync(aliasOrModelId, force: true);
        var unloaded = (await manager.ListLoadedModelsAsync())
            .Any(m => m.ModelId == model.ModelId);
        Console.WriteLine($"Model unloaded (expected: True): {!unloaded}");
    }


    private async Task TestDownload(string aliasOrModelId)
    {
        using var manager = new FoundryLocalManager();

        // Download a model
        var model = await manager.DownloadModelAsync(aliasOrModelId, force: true);

        // test that the model can be loaded
        Console.WriteLine($"Downloaded model: {model!.Alias} ({model.ModelId})");
    }
}
