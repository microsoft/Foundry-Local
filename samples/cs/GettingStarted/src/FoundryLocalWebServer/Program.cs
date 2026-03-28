using Microsoft.AI.Foundry.Local;
using OpenAI;
using System.ClientModel;

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
    Web = new Configuration.WebService
    {
        Urls = "http://127.0.0.1:52495"
    }
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


// Get a model using an alias
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


// Start the web service
Console.Write($"Starting web service on {config.Web.Urls}...");
await mgr.StartWebServiceAsync();
Console.WriteLine("done.");

// <<<<<< OPEN AI SDK USAGE >>>>>>
// Use the OpenAI SDK to call the local Foundry web service

ApiKeyCredential key = new ApiKeyCredential("notneeded");
OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = new Uri(config.Web.Urls + "/v1"),
});

var chatClient = client.GetChatClient(model.Id);
var completionUpdates = chatClient.CompleteChatStreaming("Why is the sky blue?");

Console.Write($"[ASSISTANT]: ");
foreach (var completionUpdate in completionUpdates)
{
    if (completionUpdate.ContentUpdate.Count > 0)
    {
        Console.Write(completionUpdate.ContentUpdate[0].Text);
    }
}
Console.WriteLine();
// <<<<<< END OPEN AI SDK USAGE >>>>>>

// Tidy up
// Stop the web service and unload model
await mgr.StopWebServiceAsync();
await model.UnloadAsync();