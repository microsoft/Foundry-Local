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


// Model catalog operations
// In this section of the code we demonstrate the various model catalog operations
// Get the model catalog object
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

// List cached models (i.e. downloaded models) from the catalog
var cachedModels = await catalog.GetCachedModelsAsync();
Console.WriteLine("\nCached models:");
foreach (var cachedModel in cachedModels)
{
    Console.WriteLine($"- {cachedModel.Alias} ({cachedModel.Id})");
}


// Get a model using an alias from the catalog
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

// `model.SelectedVariant` indicates which variant will be used by default.
//
// Models in Model.Variants are ordered by priority, with the highest priority first.
// The first downloaded model is selected by default.
// The highest priority is selected if no models have been downloaded.
// If the selected variant is not the highest priority, it means that Foundry Local
// has found a locally cached variant for you to improve performance (remove need to download).
Console.WriteLine("\nThe default selected model variant is: " + model.Id);
if (model.SelectedVariant != model.Variants.First())
{
    Debug.Assert(await model.SelectedVariant.IsCachedAsync());
    Console.WriteLine("The model variant was selected due to being locally cached.");
}


// OPTIONAL: `model` can be used directly and `model.SelectedVariant` will be used as the default.
//           You can explicitly select or use a specific ModelVariant if you want more control
//           over the device and/or execution provider used.
//           Model and ModelVariant can be used interchangeably in methods such as
//           DownloadAsync, LoadAsync, UnloadAsync and GetChatClientAsync.
//
// Choices:
//   - Use a ModelVariant directly from the catalog if you know the variant Id
//     - `var modelVariant = await catalog.GetModelVariantAsync("qwen2.5-0.5b-instruct-generic-gpu:3")`
//
//   - Get the ModelVariant from Model.Variants
//     - `var modelVariant = model.Variants.First(v => v.Id == "qwen2.5-0.5b-instruct-generic-cpu:4")`
//     - `var modelVariant = model.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.GPU)`
//       - optional: update selected variant in `model` using `model.SelectVariant(modelVariant);` if you wish to use
//                   `model` in your code.

// For this example we explicitly select the CPU variant, and call SelectVariant so all the following example code
// uses the `model` instance.
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


// List loaded models (i.e. in memory) from the catalog
var loadedModels = await catalog.GetLoadedModelsAsync();
Console.WriteLine("\nLoaded models:");
foreach (var loadedModel in loadedModels)
{
    Console.WriteLine($"- {loadedModel.Alias} ({loadedModel.Id})");
}
Console.WriteLine();


// Get a chat client
var chatClient = await model.GetChatClientAsync();

// Create a chat message
List<ChatMessage> messages = new()
{
    new ChatMessage { Role = "user", Content = "Why is the sky blue?" }
};

// You can adjust settings on the chat client
chatClient.Settings.Temperature = 0.7f;
chatClient.Settings.MaxTokens = 512;

Console.WriteLine("Chat completion response:");
var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in streamingResponse)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();
Console.WriteLine();

// Tidy up - unload the model
Console.WriteLine($"Unloading model {model.Id}...");
await model.UnloadAsync();
Console.WriteLine("Model unloaded.");

// Show loaded models from the catalog after unload
loadedModels = await catalog.GetLoadedModelsAsync();
Console.WriteLine("\nLoaded models after unload (will be empty):");
foreach (var loadedModel in loadedModels)
{
    Console.WriteLine($"- {loadedModel.Alias} ({loadedModel.Id})");
}
Console.WriteLine();
Console.WriteLine("Sample complete.");