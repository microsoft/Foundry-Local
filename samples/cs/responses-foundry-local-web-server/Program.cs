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
// pointed at the local web service, mirroring how `samples/cs/foundry-local-web-server`
// uses `OpenAIClient.GetChatClient(...)` for chat completions.

using System.ClientModel;

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

try
{
    // <<<<<< OPEN AI RESPONSES SDK USAGE >>>>>>
    // Use the OpenAI Responses client to call the local Foundry web service.
    ApiKeyCredential key = new("notneeded");
    OpenAIClient openai = new(key, new OpenAIClientOptions
    {
        Endpoint = new Uri(config.Web.Urls + "/v1"),
    });
    ResponsesClient responses = openai.GetResponsesClient();

    // 1) Non-streaming
    Console.WriteLine("\n=== Non-streaming ===");
    ResponseResult simple = await responses.CreateResponseAsync(model.Id, "Reply with one short sentence about local AI.");
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

    // 3) Function/tool calling — full round-trip via previous_response_id.
    // The function takes no arguments, which matches the pattern small models handle reliably.
    Console.WriteLine("\n=== Function calling ===");
    var emptyParamsSchema = BinaryData.FromString("""
        {
            "type": "object",
            "properties": {},
            "additionalProperties": false
        }
        """);

    ResponseTool getWeatherTool = ResponseTool.CreateFunctionTool(
        functionName: "get_weather",
        functionParameters: emptyParamsSchema,
        strictModeEnabled: true,
        functionDescription: "Get the current weather. This sample always returns Seattle weather.");

    var toolCallOptions = new CreateResponseOptions(
        model.Id,
        new[] { ResponseItem.CreateUserMessageItem("Use the get_weather tool and then answer with the weather.") })
    {
        StoredOutputEnabled = true,
        ToolChoice = ResponseToolChoice.CreateRequiredChoice(),
        MaxOutputTokenCount = 64,
        Temperature = 0.0f,
    };
    toolCallOptions.Tools.Add(getWeatherTool);

    ResponseResult toolResponse = await responses.CreateResponseAsync(toolCallOptions);

    FunctionCallResponseItem? functionCall = null;
    foreach (var item in toolResponse.OutputItems)
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
        Console.WriteLine($"[TOOL CALL]: {functionCall.FunctionName}({functionCall.FunctionArguments})");

        const string toolOutput = """{"location": "Seattle", "weather": "72 degrees F and sunny"}""";

        var followUpOptions = new CreateResponseOptions(
            model.Id,
            new[] { ResponseItem.CreateFunctionCallOutputItem(functionCall.CallId, toolOutput) })
        {
            PreviousResponseId = toolResponse.Id,
            StoredOutputEnabled = true,
            MaxOutputTokenCount = 64,
            Temperature = 0.0f,
        };
        followUpOptions.Tools.Add(getWeatherTool);

        ResponseResult finalResponse = await responses.CreateResponseAsync(followUpOptions);
        Console.WriteLine($"[ASSISTANT FINAL]: {finalResponse.GetOutputText()}");
    }
    // <<<<<< END OPEN AI RESPONSES SDK USAGE >>>>>>
}
finally
{
    // Tidy up
    await mgr.StopWebServiceAsync();
    await model.UnloadAsync();
}
// </complete_code>
