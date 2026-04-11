/// <summary>
/// Foundry Local SDK - WinML 2.0 EP Verification (C#)
///
/// Verifies:
///   1. WinML execution providers are discovered and registered
///   2. GPU models appear in catalog after EP registration
///   3. Streaming chat completions work on a WinML-accelerated model
///   4. OpenAI SDK chat completions work against a WinML-loaded model
/// </summary>

using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;
using OpenAI.Chat;

const string PASS = "\x1b[92m[PASS]\x1b[0m";
const string FAIL = "\x1b[91m[FAIL]\x1b[0m";
const string INFO = "\x1b[94m[INFO]\x1b[0m";

var results = new List<(string Name, bool Passed)>();

void LogResult(string testName, bool passed, string detail = "")
{
    var status = passed ? PASS : FAIL;
    var msg = string.IsNullOrEmpty(detail) ? $"{status} {testName}" : $"{status} {testName} - {detail}";
    Console.WriteLine(msg);
    results.Add((testName, passed));
}

void PrintSeparator(string title)
{
    Console.WriteLine($"\n{new string('=', 60)}");
    Console.WriteLine($"  {title}");
    Console.WriteLine($"{new string('=', 60)}\n");
}

void PrintSummary()
{
    PrintSeparator("Summary");
    var passed = results.Count(r => r.Passed);
    foreach (var (name, p) in results)
        Console.WriteLine($"  {(p ? "✓" : "✗")} {name}");
    Console.WriteLine($"\n  {passed}/{results.Count} tests passed");
}

CancellationToken ct = CancellationToken.None;

// ── 0. Initialize FoundryLocalManager ──────────────────────
PrintSeparator("Initialization");
var config = new Configuration
{
    AppName = "verify_winml",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

using var loggerFactory = LoggerFactory.Create(builder =>
    builder.SetMinimumLevel(Microsoft.Extensions.Logging.LogLevel.Information));
var logger = loggerFactory.CreateLogger<Program>();

await FoundryLocalManager.CreateAsync(config, logger);
var mgr = FoundryLocalManager.Instance;
Console.WriteLine($"{INFO} FoundryLocalManager initialized.");

// ── 1. Discover & Register EPs ────────────────────────────
PrintSeparator("Step 1: Discover & Register Execution Providers");
try
{
    var eps = mgr.DiscoverEps();
    Console.WriteLine($"{INFO} Discovered {eps.Length} execution providers:");
    foreach (var ep in eps)
    {
        Console.WriteLine($"  - {ep.Name,-40}  Registered: {ep.IsRegistered}");
    }
    LogResult("EP Discovery", true, $"{eps.Length} EP(s) found");
}
catch (Exception e)
{
    LogResult("EP Discovery", false, e.Message);
}

try
{
    var epResult = await mgr.DownloadAndRegisterEpsAsync(
        new Action<string, double>((epName, percent) =>
            Console.Write($"\r  Downloading {epName}: {percent:F1}%")), ct);
    Console.WriteLine();
    Console.WriteLine($"{INFO} EP registration: success={epResult.Success}, status={epResult.Status}");
    if (epResult.RegisteredEps?.Any() == true)
        Console.WriteLine($"  Registered: {string.Join(", ", epResult.RegisteredEps)}");
    if (epResult.FailedEps?.Any() == true)
        Console.WriteLine($"  Failed:     {string.Join(", ", epResult.FailedEps)}");
    LogResult("EP Download & Registration", epResult.Success);
}
catch (Exception e)
{
    Console.WriteLine();
    LogResult("EP Download & Registration", false, e.Message);
}

// ── 2. List Models & Find GPU Variants ────────────────────
PrintSeparator("Step 2: Model Catalog - GPU Models");
var catalog = await mgr.GetCatalogAsync();
var models = await catalog.ListModelsAsync();
Console.WriteLine($"{INFO} Total models in catalog: {models.Count}");

IModel? chosen = null;
foreach (var model in models)
{
    foreach (var variant in model.Variants)
    {
        var rt = variant.Info?.Runtime;
        if (rt?.DeviceType == DeviceType.GPU)
        {
            Console.WriteLine($"  - {variant.Id,-50}  EP: {rt.ExecutionProvider ?? "?"}");
            chosen ??= variant;
        }
    }
}

LogResult("Catalog - GPU models found", chosen != null,
    chosen != null ? $"Selected: {chosen.Id}" : "No GPU models");

if (chosen == null)
{
    Console.WriteLine($"\n{FAIL} No GPU models available. Cannot proceed with inference tests.");
    PrintSummary();
    return;
}

// ── 3. Download & Load Model ──────────────────────────────
PrintSeparator("Step 3: Download & Load Model");
try
{
    await chosen.DownloadAsync(progress =>
        Console.Write($"\r  Downloading model: {progress:F1}%"));
    Console.WriteLine();
    LogResult("Model Download", true);
}
catch (Exception e)
{
    Console.WriteLine();
    LogResult("Model Download", false, e.Message);
    PrintSummary();
    return;
}

try
{
    await chosen.LoadAsync();
    LogResult("Model Load", true, $"Loaded {chosen.Id}");
}
catch (Exception e)
{
    LogResult("Model Load", false, e.Message);
    PrintSummary();
    return;
}

// ── 4. Streaming Chat Completions (Native SDK) ────────────
PrintSeparator("Step 4: Streaming Chat Completions (Native)");
try
{
    var chatClient = await chosen.GetChatClientAsync();
    var messages = new List<Betalgo.Ranul.OpenAI.ObjectModels.RequestModels.ChatMessage>
    {
        new() { Role = "system", Content = "You are a helpful assistant." },
        new() { Role = "user", Content = "What is 2 + 2? Reply with just the number." },
    };

    var fullResponse = "";
    var start = DateTime.UtcNow;
    await foreach (var chunk in chatClient.CompleteChatStreamingAsync(messages, ct))
    {
        var content = chunk.Choices[0].Message.Content;
        if (!string.IsNullOrEmpty(content))
        {
            Console.Write(content);
            Console.Out.Flush();
            fullResponse += content;
        }
    }
    var elapsed = (DateTime.UtcNow - start).TotalSeconds;
    Console.WriteLine();
    LogResult("Streaming Chat (Native)", fullResponse.Length > 0,
        $"{fullResponse.Length} chars in {elapsed:F2}s");
}
catch (Exception e)
{
    LogResult("Streaming Chat (Native)", false, e.Message);
}

// ── 5. OpenAI SDK Chat Completions ────────────────────────
PrintSeparator("Step 5: Chat Completions (OpenAI SDK)");
try
{
    await mgr.StartWebServiceAsync();
    var webUrl = mgr.Urls?.FirstOrDefault()
        ?? throw new Exception("Web service did not return a URL");
    Console.WriteLine($"{INFO} Web service at: {webUrl}");

    var oaiClient = new ChatClient(
        model: chosen.Id,
        credential: new System.ClientModel.ApiKeyCredential("not-needed"),
        options: new OpenAI.OpenAIClientOptions { Endpoint = new Uri($"{webUrl}/v1") }
    );

    var oaiMessages = new List<ChatMessage>
    {
        new SystemChatMessage("You are a helpful assistant."),
        new UserChatMessage("Name three colors. Reply briefly."),
    };

    var response = await oaiClient.CompleteChatAsync(oaiMessages, cancellationToken: ct);
    var content = response.Value.Content[0].Text ?? "";
    Console.WriteLine($"  Response: {content[..Math.Min(content.Length, 200)]}");
    LogResult("Chat (OpenAI SDK)", content.Length > 0, $"{content.Length} chars");
}
catch (Exception e)
{
    LogResult("Chat (OpenAI SDK)", false, e.Message);
}

// ── Summary ──────────────────────────────────────────────
PrintSummary();

await chosen.UnloadAsync();
Console.WriteLine("Model unloaded. Done!");
