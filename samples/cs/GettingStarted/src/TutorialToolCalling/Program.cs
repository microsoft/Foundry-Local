// <complete_code>
// <imports>
using System.Data;
using System.Text.Json;
using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Microsoft.Extensions.Logging;
// </imports>

CancellationToken ct = CancellationToken.None;

// <tool_definitions>
// --- Tool definitions ---
var tools = new[]
{
    new
    {
        type = "function",
        function = new
        {
            name = "get_weather",
            description = "Get the current weather for a location",
            parameters = new
            {
                type = "object",
                properties = new
                {
                    location = new
                    {
                        type = "string",
                        description = "The city or location"
                    },
                    unit = new
                    {
                        type = "string",
                        @enum = new[] { "celsius", "fahrenheit" },
                        description = "Temperature unit"
                    }
                },
                required = new[] { "location" }
            }
        }
    },
    new
    {
        type = "function",
        function = new
        {
            name = "calculate",
            description = "Perform a math calculation",
            parameters = new
            {
                type = "object",
                properties = new
                {
                    expression = new
                    {
                        type = "string",
                        description =
                            "The math expression to evaluate"
                    }
                },
                required = new[] { "expression" }
            }
        }
    }
};

// --- Tool implementations ---
string ExecuteTool(string functionName, JsonElement arguments)
{
    switch (functionName)
    {
        case "get_weather":
            var location = arguments.GetProperty("location")
                .GetString() ?? "unknown";
            var unit = arguments.TryGetProperty("unit", out var u)
                ? u.GetString() ?? "celsius"
                : "celsius";
            var temp = unit == "celsius" ? 22 : 72;
            return JsonSerializer.Serialize(new
            {
                location,
                temperature = temp,
                unit,
                condition = "Sunny"
            });

        case "calculate":
            var expression = arguments.GetProperty("expression")
                .GetString() ?? "";
            try
            {
                var result = new DataTable()
                    .Compute(expression, null);
                return JsonSerializer.Serialize(new
                {
                    expression,
                    result = result?.ToString()
                });
            }
            catch (Exception ex)
            {
                return JsonSerializer.Serialize(new
                {
                    error = ex.Message
                });
            }

        default:
            return JsonSerializer.Serialize(new
            {
                error = $"Unknown function: {functionName}"
            });
    }
}

string ProcessToolCalls(
    List<ChatMessage> msgs,
    Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels.ChatCompletionResponse resp,
    dynamic client)
{
    var choice = resp.Choices[0].Message;

    while (choice.ToolCalls is { Count: > 0 })
    {
        msgs.Add(choice);

        foreach (var toolCall in choice.ToolCalls)
        {
            var args = JsonDocument.Parse(
                toolCall.FunctionCall.Arguments
            ).RootElement;
            Console.WriteLine(
                $"  Tool call: {toolCall.FunctionCall.Name}({args})"
            );

            var result = ExecuteTool(
                toolCall.FunctionCall.Name, args
            );
            msgs.Add(new ChatMessage
            {
                Role = "tool",
                ToolCallId = toolCall.Id,
                Content = result
            });
        }

        resp = ((Task<Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels
            .ChatCompletionResponse>)client
            .CompleteChatAsync(msgs, ct, tools)).Result;
        choice = resp.Choices[0].Message;
    }

    return choice.Content ?? "";
}
// </tool_definitions>

// <init>
// --- Main application ---
var config = new Configuration
{
    AppName = "tool-calling-app",
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

var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync("phi-3.5-mini")
    ?? throw new Exception("Model not found");

await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await model.LoadAsync();
Console.WriteLine("Model loaded and ready.");

var chatClient = await model.GetChatClientAsync();

var messages = new List<ChatMessage>
{
    new ChatMessage
    {
        Role = "system",
        Content = "You are a helpful assistant with access to tools. " +
                  "Use them when needed to answer questions accurately."
    }
};
// </init>

// <tool_loop>
Console.WriteLine("\nTool-calling assistant ready! Type 'quit' to exit.\n");

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
        messages, ct, tools
    );

    var choice = response.Choices[0].Message;

    if (choice.ToolCalls is { Count: > 0 })
    {
        messages.Add(choice);

        foreach (var toolCall in choice.ToolCalls)
        {
            var args = JsonDocument.Parse(
                toolCall.FunctionCall.Arguments
            ).RootElement;
            Console.WriteLine(
                $"  Tool call: {toolCall.FunctionCall.Name}({args})"
            );

            var result = ExecuteTool(
                toolCall.FunctionCall.Name, args
            );
            messages.Add(new ChatMessage
            {
                Role = "tool",
                ToolCallId = toolCall.Id,
                Content = result
            });
        }

        var finalResponse = await chatClient.CompleteChatAsync(
            messages, ct, tools
        );
        var answer = finalResponse.Choices[0].Message.Content ?? "";
        messages.Add(new ChatMessage
        {
            Role = "assistant",
            Content = answer
        });
        Console.WriteLine($"Assistant: {answer}\n");
    }
    else
    {
        var answer = choice.Content ?? "";
        messages.Add(new ChatMessage
        {
            Role = "assistant",
            Content = answer
        });
        Console.WriteLine($"Assistant: {answer}\n");
    }
}

await model.UnloadAsync();
Console.WriteLine("Model unloaded. Goodbye!");
// </tool_loop>
// </complete_code>
