using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

// ---------------------------------------------------------------------------
// Private Catalog sample (placeholder).
//
// The full private-catalog flow (JWT signing, AddOrUpdateCatalogAsync, token
// refresh, IsFromCatalogRegistry filtering, GetModelAsync(alias, preferRegistry))
// depends on SDK APIs that ship in Microsoft.AI.Foundry.Local 1.1.0-dev or later.
// Until that package is published to the public feed, this Program.cs is kept
// minimal so the sample compiles against the currently released SDK.
//
// See README.md for the full private-catalog workflow and JWT signing helper.
// ---------------------------------------------------------------------------

CancellationToken ct = default;

var config = new Configuration
{
    AppName = "private_catalog_sample",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
};

await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

var catalog = await mgr.GetCatalogAsync();

Console.WriteLine("Available models:");
var models = await catalog.ListModelsAsync();
foreach (var m in models)
{
    foreach (var v in m.Variants)
    {
        Console.WriteLine($"  - {v.Alias} ({v.Id})");
    }
}

var model = await catalog.GetModelAsync("qwen2.5-0.5b")
    ?? throw new Exception("Model not found");

await model.DownloadAsync(p =>
{
    Console.Write($"\rDownloading: {p:F1}%");
    if (p >= 100f) Console.WriteLine();
});

Console.Write($"Loading {model.Id}...");
await model.LoadAsync();
Console.WriteLine(" done.");

var chat = await model.GetChatClientAsync();
var messages = new List<ChatMessage> { new() { Role = "user", Content = "Why is the sky blue?" } };

Console.WriteLine("Chat completion:");
await foreach (var chunk in chat.CompleteChatStreamingAsync(messages, ct))
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

await model.UnloadAsync();
