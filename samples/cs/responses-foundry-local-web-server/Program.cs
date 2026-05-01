// <complete_code>
// Demonstrates the OpenAI Responses API against the Foundry Local OpenAI-compatible web service.
//
// SDK responsibilities (Foundry Local):
//   - SDK initialization
//   - EP download/registration
//   - model lookup, download, load
//   - starting/stopping the local web service
//
// Responses API calls go through the official OpenAI .NET package's `ResponsesClient`
// pointed at the local web service, mirroring how `foundry-local-web-server` uses
// `OpenAIClient.GetChatClient(...)`.

using System.ClientModel;
using System.Text;
using System.Text.Json;

using Microsoft.AI.Foundry.Local;

using OpenAI;
using OpenAI.Responses;

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

// Get the model catalog
var catalog = await mgr.GetCatalogAsync();

// Get a model using an alias
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

// Start the web service
Console.Write($"Starting web service on {config.Web.Urls}...");
await mgr.StartWebServiceAsync();
Console.WriteLine("done.");

// <<<<<< OPEN AI RESPONSES SDK USAGE >>>>>>
// Use the OpenAI Responses client to call the local Foundry web service.
ApiKeyCredential key = new ApiKeyCredential("notneeded");
OpenAIClient openai = new OpenAIClient(key, new OpenAIClientOptions
{
    Endpoint = new Uri(config.Web.Urls + "/v1"),
});
ResponsesClient responses = openai.GetResponsesClient();

// 1) Non-streaming
Console.WriteLine("\n=== Non-streaming ===");
ResponseResult simple = await responses.CreateResponseAsync(model.Id, "What is 2 + 2? Respond with just the number.");
Console.WriteLine($"[ASSISTANT]: {simple.GetOutputText()}");

// 2) Streaming
Console.WriteLine("\n=== Streaming ===");
Console.Write("[ASSISTANT]: ");
await foreach (StreamingResponseUpdate update in responses.CreateResponseStreamingAsync(model.Id, "Count from 1 to 3."))
{
    if (update is StreamingResponseOutputTextDeltaUpdate delta && !string.IsNullOrEmpty(delta.Delta))
    {
        Console.Write(delta.Delta);
    }
}
Console.WriteLine();

// 3) Function/tool calling — full round-trip using previous_response_id.
Console.WriteLine("\n=== Function calling ===");
var weatherSchema = BinaryData.FromString("""
    {
        "type": "object",
        "properties": {
            "city": { "type": "string", "description": "The city to look up" }
        },
        "required": ["city"]
    }
    """);

var toolOptions = new CreateResponseOptions(
    model.Id,
    new[] { ResponseItem.CreateUserMessageItem("Use get_weather to look up the weather in Seattle, then summarize it.") })
{
    StoredOutputEnabled = true,
    ToolChoice = ResponseToolChoice.CreateRequiredChoice(),
};
toolOptions.Tools.Add(ResponseTool.CreateFunctionTool(
    functionName: "get_weather",
    functionParameters: weatherSchema,
    strictModeEnabled: true,
    functionDescription: "Get the current weather for a given city."));

ResponseResult toolCallResponse = await responses.CreateResponseAsync(toolOptions);

// Find the function-call output item the model produced.
FunctionCallResponseItem? functionCall = null;
foreach (var item in toolCallResponse.OutputItems)
{
    if (item is FunctionCallResponseItem fc && fc.FunctionName == "get_weather")
    {
        functionCall = fc;
        break;
    }
}

if (functionCall is null)
{
    Console.WriteLine("Model did not produce a function call; skipping tool round-trip.");
}
else
{
    var argsJson = functionCall.FunctionArguments?.ToString() ?? "{}";
    var city = "unknown";
    try
    {
        city = JsonDocument.Parse(argsJson).RootElement.GetProperty("city").GetString() ?? "unknown";
    }
    catch (KeyNotFoundException) { /* model gave us no city */ }

    Console.WriteLine($"Tool call: get_weather(city=\"{city}\")");
    var toolOutput = $$$"""{"city": "{{{city}}}", "temperatureF": 68, "summary": "partly cloudy"}""";
    Console.WriteLine($"Tool output: {toolOutput}");

    // Submit the tool's output and ask the model to continue using `previous_response_id`.
    var followUpOptions = new CreateResponseOptions(
        model.Id,
        new[] { ResponseItem.CreateFunctionCallOutputItem(functionCall.CallId, toolOutput) })
    {
        PreviousResponseId = toolCallResponse.Id,
        StoredOutputEnabled = true,
    };
    followUpOptions.Tools.Add(ResponseTool.CreateFunctionTool(
        functionName: "get_weather",
        functionParameters: weatherSchema,
        strictModeEnabled: true,
        functionDescription: "Get the current weather for a given city."));

    ResponseResult finalResponse = await responses.CreateResponseAsync(followUpOptions);
    Console.WriteLine($"[ASSISTANT]: {finalResponse.GetOutputText()}");
}
// <<<<<< END OPEN AI RESPONSES SDK USAGE >>>>>>

// Tidy up
await mgr.StopWebServiceAsync();
await model.UnloadAsync();
// </complete_code>
