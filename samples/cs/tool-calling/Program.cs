using System.Text.Json;
using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging.Abstractions;

var configuration = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = LogLevel.Information,
};

await FoundryLocalManager.CreateAsync(configuration, NullLogger.Instance).ConfigureAwait(false);
using var manager = FoundryLocalManager.Instance;

var catalog = await manager.GetCatalogAsync().ConfigureAwait(false);
var model = await catalog.GetModelAsync("qwen2.5-0.5b").ConfigureAwait(false)
    ?? throw new InvalidOperationException("The qwen2.5-0.5b model was not found.");
if (model.Info.SupportsToolCalling != true)
{
    throw new InvalidOperationException("The selected qwen2.5-0.5b variant does not support tool calling.");
}

Console.WriteLine($"Using tool-calling model: {model.Id}");

if (!await model.IsCachedAsync().ConfigureAwait(false))
{
    Console.WriteLine($"Downloading {model.Id}...");
    await model.DownloadAsync(
        progress => Console.Write($"\r  {progress:F1}%   "))
        .ConfigureAwait(false);
    Console.WriteLine();
}

await model.LoadAsync().ConfigureAwait(false);

try
{
    using var session = new ChatSession(model);
    session.AddToolDefinition(
        "multiply_numbers",
        "Multiply two integer values.",
        """
        {
          "type": "object",
          "properties": {
            "first": { "type": "integer" },
            "second": { "type": "integer" }
          },
          "required": ["first", "second"]
        }
        """);

    using var firstRequest = new Request();
    firstRequest.AddItem(MessageItem.System("Use the provided tool to perform arithmetic."));
    firstRequest.AddItem(MessageItem.User("What is 7 multiplied by 6?"));
    firstRequest.SetOptions(new RequestOptions
    {
        Search = new SearchOptions { Temperature = 0.0f },
        ToolChoice = ToolChoice.Required,
    });

    using var firstResponse = await session.ProcessRequestAsync(firstRequest).ConfigureAwait(false);
    var toolCall = GetToolCall(firstResponse);

    using var arguments = JsonDocument.Parse(toolCall.Arguments);
    var first = arguments.RootElement.GetProperty("first").GetInt32();
    var second = arguments.RootElement.GetProperty("second").GetInt32();
    var result = first * second;

    Console.WriteLine($"Tool call: {toolCall.Name}({first}, {second})");
    Console.WriteLine($"Tool result: {result}");

    using var secondRequest = new Request();
    secondRequest.AddItem(new ToolResultItem(toolCall.CallId, result.ToString()));
    secondRequest.AddItem(MessageItem.System("Reply with only the result returned by the tool."));

    using var secondResponse = await session.ProcessRequestAsync(secondRequest).ConfigureAwait(false);
    foreach (var item in secondResponse)
    {
        using (item)
        {
            if (item is MessageItem message)
            {
                Console.WriteLine($"Assistant: {message.GetSimpleText()}");
            }
        }
    }
}
finally
{
    await model.UnloadAsync().ConfigureAwait(false);
}

static (string CallId, string Name, string Arguments) GetToolCall(Response response)
{
    foreach (var item in response)
    {
        using (item)
        {
            if (item is ToolCallItem toolCall)
            {
                return (toolCall.CallId, toolCall.Name, toolCall.Arguments);
            }
        }
    }

    throw new InvalidOperationException("The model did not return a tool call.");
}
