// <complete_code>
// This sample demonstrates two ways to run the same chat prompt against Foundry Local:
//   1. Native, in-process inference via the SDK's chat client.
//   2. The local OpenAI-compatible web server (/v1/chat/completions) via the OpenAI SDK.
//
// <imports>
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using OpenAI;
using System.ClientModel;
// </imports>

// The same prompt is used for both the native and web-server demonstrations.
const string prompt = "Why is the sky blue?";

// <init>
CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
    // The web server is started later for the second demonstration.
    Web = new Configuration.WebService
    {
        Urls = "http://127.0.0.1:52495"
    }
};


// Initialize the singleton instance.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;


// Discover available execution providers and their registration status.
var eps = mgr.DiscoverEps();
int maxNameLen = 30;
Console.WriteLine("Available execution providers:");
Console.WriteLine($"  {"Name".PadRight(maxNameLen)}  Registered");
Console.WriteLine($"  {new string('─', maxNameLen)}  {"──────────"}");
foreach (var ep in eps)
{
    Console.WriteLine($"  {ep.Name.PadRight(maxNameLen)}  {ep.IsRegistered}");
}

// Download and register all execution providers with per-EP progress.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
// For cross platform builds there is no dynamic EP download and this will return immediately.
Console.WriteLine("\nDownloading execution providers:");
if (eps.Length > 0)
{
    string currentEp = "";
    await mgr.DownloadAndRegisterEpsAsync((epName, percent) =>
    {
        if (epName != currentEp)
        {
            if (currentEp != "")
            {
                Console.WriteLine();
            }
            currentEp = epName;
        }
        Console.Write($"\r  {epName.PadRight(maxNameLen)}  {percent,6:F1}%");
    });
    Console.WriteLine();
}
else
{
    Console.WriteLine("No execution providers to download.");
}
// </init>


// <model_setup>
// Get the model catalog
var catalog = await mgr.GetCatalogAsync();


// Get a model using an alias.
var model = await catalog.GetModelAsync("qwen2.5-0.5b") ?? throw new Exception("Model not found");

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
Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");
// </model_setup>

// <native_inference>
// === Native inference ===
// Run the prompt with the SDK's in-process chat client (no web server involved).
Console.WriteLine();
Console.WriteLine("=== Native inference ===");

var chatClient = await model.GetChatClientAsync();

List<ChatMessage> messages = new()
{
    new ChatMessage { Role = "user", Content = prompt }
};

Console.Write("[ASSISTANT]: ");
var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in streamingResponse)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();
// </native_inference>

// <web_server>
// === Web server (/v1/chat/completions) ===
// Run the same prompt against the local OpenAI-compatible web server using the OpenAI SDK.
Console.WriteLine();
Console.WriteLine("=== Web server (/v1/chat/completions) ===");

Console.Write($"Starting web service on {config.Web.Urls}...");
await mgr.StartWebServiceAsync();
Console.WriteLine("done.");

// Use the OpenAI SDK to call the local Foundry web service.
ApiKeyCredential key = new ApiKeyCredential("notneeded");
OpenAIClient client = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = new Uri(config.Web.Urls + "/v1"),
});

var webChatClient = client.GetChatClient(model.Id);
var completionUpdates = webChatClient.CompleteChatStreaming(prompt);

Console.Write("[ASSISTANT]: ");
foreach (var completionUpdate in completionUpdates)
{
    if (completionUpdate.ContentUpdate.Count > 0)
    {
        Console.Write(completionUpdate.ContentUpdate[0].Text);
    }
}
Console.WriteLine();

// Stop the web service.
await mgr.StopWebServiceAsync();
// </web_server>

// <cleanup>
// Tidy up - unload the model
await model.UnloadAsync();
// </cleanup>
// </complete_code>
