using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using System.Diagnostics;

CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
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


// List available models
Console.WriteLine("Available models for your hardware:");
var models = await catalog.ListModelsAsync();
foreach (var availableModel in models)
{
    foreach (var variant in availableModel.Variants)
    {
        Console.WriteLine($"  - Alias: {variant.Alias} (Id: {string.Join(", ", variant.Id)})");
    }
}


// print out cached models
var cachedModels = await catalog.GetCachedModelsAsync();
Console.WriteLine("\nCached models:");
foreach (var cachedModel in cachedModels)
{
    Console.WriteLine($"- {cachedModel.Alias} ({cachedModel.Id})");
}


// Get a model using an alias.
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

// `model.SelectedVariant` indicates which variant will be used by default.
//
// Models in Model.Variants are ordered by priority, with the highest priority first.
// The first downloaded model is selected by default.
// The highest priority is selected if no models have been downloaded.
Console.WriteLine("\nThe default selected model variant is: " + model.Id);
if (model.SelectedVariant != model.Variants.First())
{
    Debug.Assert(await model.SelectedVariant.IsCachedAsync());
    Console.WriteLine("The model variant was selected due to being locally cached.");
}


// OPTIONAL: `model` can be used directly and `model.SelectedVariant` will be used as the default.
//           You can explicitly select or use a specific ModelVariant if you want more control
//           over the device and/or execution provider used.
//
// Options:
//   - Use a ModelVariant directly from the catalog if you know the variant Id
//     - `var modelVariant = await catalog.GetModelVariantAsync("qwen2.5-0.5b-instruct-generic-gpu:3")`
//
//   - Get the ModelVariant from Model.Variants
//     - `var modelVariant = model.Variants.First(v => v.Id == "qwen2.5-0.5b-instruct-generic-cpu:4")`
//     - `var modelVariant = model.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.GPU)`
//     - Update selected variant in `model`: `model.SelectVariant(modelVariant);`

// For this example we explicitly select the CPU variant, and call SelectVariant so all the following example code
// uses the `model` instance.
// You can also use modelVariant for download/load/get client/unload.
Console.WriteLine("Selecting CPU variant of model");
var modelVariant = model.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
model.SelectVariant(modelVariant);


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
await model.LoadAsync();

// Get a chat client
var chatClient = await model.GetChatClientAsync();

// Create a chat message
List<ChatMessage> messages = new()
{
    new ChatMessage { Role = "user", Content = "Why is the sky blue?" }
};

// You can adjust settings on the chat client
chatClient.Settings.Temperature = 0.7f;
chatClient.Settings.N = 512;

var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in streamingResponse)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

// Tidy up - unload the model
await model.UnloadAsync();