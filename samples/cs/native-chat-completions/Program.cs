// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
// </imports>

// <init>
CancellationToken ct = new CancellationToken();

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
    // Pre-configure the web service URL so the BYOM bonus block at the bottom
    // works without changing the manager configuration. Harmless for the
    // catalog flow because the web service is only started on demand.
    Web = new Configuration.WebService { Urls = "http://127.0.0.1:0" }
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

// <chat_completion>
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
// </chat_completion>

// <cleanup>
// Tidy up - unload the model
await model.UnloadAsync();
// </cleanup>

// ---------------------------------------------------------------------------
// Bonus: chat with a bring-your-own-model that lives in your cache directory
// but is not in the Azure catalog. Replace "my-byom-model" with your model id
// and uncomment the block below. Requires `using System.Net.Http.Json;` and
// `using System.Text.Json;`.
// ---------------------------------------------------------------------------
// const string byomId = "my-byom-model";
// await mgr.StartWebServiceAsync(ct);
// using var http = new HttpClient { BaseAddress = new Uri(mgr.Urls![0]) };
// await http.GetAsync($"/models/load/{Uri.EscapeDataString(byomId)}", ct);
// var payload = new { model = byomId, messages = new[] { new { role = "user", content = "Hello!" } } };
// var resp = await http.PostAsJsonAsync("/v1/chat/completions", payload, ct);
// using var doc = JsonDocument.Parse(await resp.Content.ReadAsStringAsync(ct));
// Console.WriteLine(doc.RootElement.GetProperty("choices")[0].GetProperty("message").GetProperty("content").GetString());
// await http.GetAsync($"/models/unload/{Uri.EscapeDataString(byomId)}", ct);
// </complete_code>