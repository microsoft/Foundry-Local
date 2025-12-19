using Microsoft.AI.Foundry.Local;
using OpenAI.Chat;

Configuration appConfiguration = new()
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};
await FoundryLocalManager.CreateAsync(appConfiguration, Utils.GetAppLogger());

// Ensure that any Execution Provider (EP) downloads run and are completed.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
// For cross platform builds there is no dynamic EP download and this will return immediately.
await Utils.RunWithSpinner("Registering execution providers", FoundryLocalManager.Instance.EnsureEpsDownloadedAsync());

// Use the model catalog to find a model via alias and download it, if not already cached
ICatalog modelCatalog = await FoundryLocalManager.Instance.GetCatalogAsync();
Model model = await modelCatalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");
await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f)
    {
        Console.WriteLine();
    }
});

Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");

// Use chat completions with the official OpenAI library, via a Foundry Local derived client
ChatClient localChatClient = new FoundryLocalChatClient(model);
ChatCompletion localCompletion = localChatClient.CompleteChat("Say hello!");
Console.WriteLine(localCompletion.Content[0].Text);

// Tidy up - unload the model
await model.UnloadAsync();