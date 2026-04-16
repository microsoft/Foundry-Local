// Foundry Local + Learn MCP Server: Local AI Doc Assistant
// Uses Foundry Local for on-device inference and Learn MCP Server for doc retrieval.

using System.Text.Json;
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;
using Betalgo.Ranul.OpenAI.ObjectModels.SharedModels;
using Microsoft.Extensions.Logging;

CancellationToken ct = CancellationToken.None;

// --- MCP endpoint ---
const string McpEndpoint = "https://learn.microsoft.com/api/mcp";

// --- Tool definitions ---
List<ToolDefinition> tools =
[
    new ToolDefinition
    {
        Type = "function",
        Function = new FunctionDefinition()
        {
            Name = "search_docs",
            Description = "Search Microsoft Learn documentation for a given query. Returns relevant documentation content with titles and URLs. Use this tool whenever the user asks about a Microsoft product, service, SDK, API, or technology.",
            Parameters = new PropertyDefinition()
            {
                Type = "object",
                Properties = new Dictionary<string, PropertyDefinition>()
                {
                    { "query", new PropertyDefinition() { Type = "string", Description = "The search query about a Microsoft product or technology" } }
                },
                Required = ["query"]
            }
        }
    }
];

// --- Tool implementation: call Learn MCP Server ---
var httpClient = new HttpClient();

async Task<string> SearchDocsAsync(string query)
{
    Console.WriteLine($"  [Searching Learn MCP Server for: \"{query}\"]");

    // MCP uses JSON-RPC over streamable HTTP
    var requestBody = JsonSerializer.Serialize(new
    {
        jsonrpc = "2.0",
        id = 1,
        method = "tools/call",
        @params = new
        {
            name = "microsoft_docs_search",
            arguments = new { query }
        }
    });

    var request = new HttpRequestMessage(HttpMethod.Post, McpEndpoint)
    {
        Content = new StringContent(requestBody, System.Text.Encoding.UTF8, "application/json")
    };
    request.Headers.Accept.ParseAdd("application/json");
    request.Headers.Accept.ParseAdd("text/event-stream");

    var response = await httpClient.SendAsync(request, ct);
    if (!response.IsSuccessStatusCode)
    {
        return JsonSerializer.Serialize(new { error = $"MCP request failed: {(int)response.StatusCode} {response.ReasonPhrase}" });
    }

    var contentType = response.Content.Headers.ContentType?.MediaType ?? "";
    var body = await response.Content.ReadAsStringAsync(ct);

    // Handle SSE/streaming response
    if (contentType.Contains("text/event-stream"))
    {
        foreach (var line in body.Split('\n'))
        {
            if (line.StartsWith("data: "))
            {
                try
                {
                    using var doc = JsonDocument.Parse(line[6..]);
                    if (doc.RootElement.TryGetProperty("result", out var result))
                    {
                        return FormatSearchResults(result);
                    }
                }
                catch { /* skip non-JSON lines */ }
            }
        }
        return JsonSerializer.Serialize(new { error = "No result found in SSE response" });
    }

    // Handle direct JSON response
    using var jsonDoc = JsonDocument.Parse(body);
    if (jsonDoc.RootElement.TryGetProperty("result", out var directResult))
    {
        return FormatSearchResults(directResult);
    }
    return JsonSerializer.Serialize(new { error = "Unexpected response format" });
}

string FormatSearchResults(JsonElement result)
{
    // MCP tool results come as content arrays
    var results = new List<string>();

    if (result.TryGetProperty("content", out var content))
    {
        foreach (var item in content.EnumerateArray())
        {
            if (item.GetProperty("type").GetString() == "text")
            {
                var text = item.GetProperty("text").GetString() ?? "";
                try
                {
                    using var parsed = JsonDocument.Parse(text);
                    if (parsed.RootElement.TryGetProperty("results", out var searchResults)
                        && searchResults.ValueKind == JsonValueKind.Array)
                    {
                        var count = 0;
                        foreach (var r in searchResults.EnumerateArray())
                        {
                            if (count++ >= 3) break;
                            var title = r.GetProperty("title").GetString() ?? "";
                            var entry = $"## {title}";
                            if (r.TryGetProperty("contentUrl", out var url))
                                entry += $"\nSource: {url.GetString()}";
                            entry += $"\n{r.GetProperty("content").GetString()}";
                            results.Add(entry);
                        }
                        continue;
                    }
                }
                catch { /* not JSON, use as-is */ }
                results.Add(text);
            }
        }
    }

    if (results.Count == 0)
    {
        return JsonSerializer.Serialize(new { message = "No documentation found for this query." });
    }

    // Truncate to ~2000 chars to fit in model context window
    var combined = string.Join("\n\n---\n\n", results);
    if (combined.Length > 2000)
    {
        combined = combined[..2000] + "\n\n[Truncated]";
    }

    return JsonSerializer.Serialize(new
    {
        documentation = combined,
        source = "Microsoft Learn (learn.microsoft.com)"
    });
}

async Task<string> ExecuteToolAsync(string functionName, JsonElement arguments)
{
    switch (functionName)
    {
        case "search_docs":
            var query = arguments.GetProperty("query").GetString() ?? "";
            return await SearchDocsAsync(query);

        default:
            return JsonSerializer.Serialize(new
            {
                error = $"Unknown function: {functionName}"
            });
    }
}

// --- Main application ---
var config = new Configuration
{
    AppName = "learn_doc_assistant",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

using var loggerFactory = LoggerFactory.Create(builder =>
{
    builder.SetMinimumLevel(
        Microsoft.Extensions.Logging.LogLevel.Information
    );
});
var logger = loggerFactory.CreateLogger<Program>();

await FoundryLocalManager.CreateAsync(config, logger);
var mgr = FoundryLocalManager.Instance;

// Download and register all execution providers.
var currentEp = "";
await mgr.DownloadAndRegisterEpsAsync((epName, percent) =>
{
    if (epName != currentEp)
    {
        if (currentEp != "") Console.WriteLine();
        currentEp = epName;
    }
    Console.Write($"\r  {epName.PadRight(30)}  {percent,6:F1}%");
});
if (currentEp != "") Console.WriteLine();

var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync("phi-4-mini")
    ?? throw new Exception("Model not found");

await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await model.LoadAsync();
Console.WriteLine("Model loaded and ready.");

var chatClient = await model.GetChatClientAsync();
chatClient.Settings.ToolChoice = ToolChoice.Required;

var messages = new List<ChatMessage>
{
    new ChatMessage
    {
        Role = "system",
        Content = "You are a Microsoft Learn documentation assistant. " +
                  "You MUST ALWAYS call the search_docs tool before answering ANY question. " +
                  "NEVER answer from your own knowledge. " +
                  "If the user asks about any Microsoft product, service, or technology, call search_docs first. " +
                  "Base your answer ONLY on the documentation returned by the tool. " +
                  "Include source URLs when available."
    }
};

Console.WriteLine("\nLearn Doc Assistant ready! Ask about any Microsoft product or technology.");
Console.WriteLine("Type 'quit' to exit.\n");

while (true)
{
    Console.Write("You: ");
    var userInput = Console.ReadLine();
    if (string.IsNullOrWhiteSpace(userInput) ||
        userInput.Equals("quit", StringComparison.OrdinalIgnoreCase) ||
        userInput.Equals("exit", StringComparison.OrdinalIgnoreCase))
    {
        break;
    }

    messages.Add(new ChatMessage
    {
        Role = "user",
        Content = userInput
    });

    var response = await chatClient.CompleteChatAsync(
        messages, tools, ct
    );

    var choice = response.Choices[0].Message;

    // Tool-calling loop: keep processing until the model produces a final answer
    while (choice.ToolCalls is { Count: > 0 })
    {
        messages.Add(choice);

        foreach (var toolCall in choice.ToolCalls)
        {
            var toolArgs = JsonDocument.Parse(
                toolCall.FunctionCall.Arguments
            ).RootElement;
            Console.WriteLine(
                $"  Tool call: {toolCall.FunctionCall.Name}({toolArgs})"
            );

            var result = await ExecuteToolAsync(
                toolCall.FunctionCall.Name, toolArgs
            );
            messages.Add(new ChatMessage
            {
                Role = "tool",
                ToolCallId = toolCall.Id,
                Content = result
            });
        }

        // Let model answer naturally on follow-up (don't force tool_choice)
        var savedToolChoice = chatClient.Settings.ToolChoice;
        chatClient.Settings.ToolChoice = ToolChoice.Auto;
        response = await chatClient.CompleteChatAsync(
            messages, tools, ct
        );
        chatClient.Settings.ToolChoice = savedToolChoice;
        choice = response.Choices[0].Message;
    }

    var answer = choice.Content ?? "";
    messages.Add(new ChatMessage
    {
        Role = "assistant",
        Content = answer
    });
    Console.WriteLine($"\nAssistant: {answer}\n");
}

await model.UnloadAsync();
Console.WriteLine("Model unloaded. Goodbye!");
