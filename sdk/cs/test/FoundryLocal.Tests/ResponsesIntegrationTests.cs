// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.ClientModel;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

using OfficialOpenAI = global::OpenAI;
using OfficialResponses = global::OpenAI.Responses;

/// <summary>
/// Integration tests for the OpenAI Responses API served by the Foundry Local web service.
///
/// The Foundry Local SDK is responsible only for model lifecycle and starting the local
/// web service. The Responses API is exercised through the official OpenAI .NET package
/// pointed at the local <c>/v1</c> endpoint.
///
/// These tests require a cached <c>qwen2.5-0.5b</c> model and a working Foundry Local
/// runtime; they are skipped automatically if the model can't be loaded locally
/// (e.g. CI without a model cache).
/// </summary>
internal sealed class ResponsesIntegrationTests
{
    private const string ModelAlias = "qwen2.5-0.5b";
    private const string ModelVariant = "qwen2.5-0.5b-instruct-generic-cpu:4";

    private static IModel? model;
    private static OfficialOpenAI.OpenAIClient? openAiClient;
    private static OfficialResponses.ResponsesClient? responses;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance; // initialized by Utils

        var catalog = await manager.GetCatalogAsync();
        var modelVariant = await catalog.GetModelVariantAsync(ModelVariant).ConfigureAwait(false);
        if (modelVariant is null)
        {
            // Model isn't in this environment's catalog; skip the suite.
            return;
        }

        if (!await modelVariant.IsCachedAsync().ConfigureAwait(false))
        {
            // Don't download in tests — leave it to the developer/CI to pre-cache.
            return;
        }

        await modelVariant.LoadAsync().ConfigureAwait(false);
        await Assert.That(await modelVariant.IsLoadedAsync()).IsTrue();
        model = modelVariant;

        await manager.StartWebServiceAsync().ConfigureAwait(false);
        await Assert.That(manager.Urls).IsNotNull().And.IsNotEmpty();

        var endpoint = new Uri(manager.Urls![0].TrimEnd('/') + "/v1");
        openAiClient = new OfficialOpenAI.OpenAIClient(new ApiKeyCredential("notneeded"), new OfficialOpenAI.OpenAIClientOptions { Endpoint = endpoint });
        responses = openAiClient.GetResponsesClient();
    }

    [After(Class)]
    public static async Task TearDown()
    {
        var manager = FoundryLocalManager.Instance;
        try
        {
            await manager.StopWebServiceAsync().ConfigureAwait(false);
        }
        catch
        {
            // best-effort cleanup
        }

        if (model is not null)
        {
            try
            {
                await model.UnloadAsync().ConfigureAwait(false);
            }
            catch
            {
                // best-effort cleanup
            }
        }
    }

    private static (OfficialResponses.ResponsesClient Client, IModel Model) RequireSetup()
    {
        if (responses is null || model is null)
        {
            Skip.Test($"Skipping: '{ModelAlias}' is not cached locally; run the SDK once to download it.");
        }

        return (responses!, model!);
    }

    [Test]
    public async Task NonStreaming_SimplePrompt_ReturnsText()
    {
        var (client, m) = RequireSetup();
        OfficialResponses.ResponseResult response = await client.CreateResponseAsync(m.Id, "What is 2 + 2? Respond with just the number.")
                                              .ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        var text = response.GetOutputText();
        Console.WriteLine($"[NonStreaming] {text}");
        await Assert.That(text).IsNotNull().And.IsNotEmpty();
    }

    [Test]
    public async Task Streaming_EmitsTextDeltaAndCompletionEvents()
    {
        var (client, m) = RequireSetup();

        var sawTextDelta = false;
        var sawCompleted = false;
        var aggregate = new StringBuilder();

        await foreach (OfficialResponses.StreamingResponseUpdate update in client.CreateResponseStreamingAsync(m.Id, "Count from 1 to 3."))
        {
            switch (update)
            {
                case OfficialResponses.StreamingResponseOutputTextDeltaUpdate delta when !string.IsNullOrEmpty(delta.Delta):
                    sawTextDelta = true;
                    aggregate.Append(delta.Delta);
                    break;
                case OfficialResponses.StreamingResponseCompletedUpdate:
                    sawCompleted = true;
                    break;
            }
        }

        Console.WriteLine($"[Streaming] aggregated: {aggregate}");
        await Assert.That(sawTextDelta).IsTrue();
        await Assert.That(sawCompleted).IsTrue();
    }

    [Test]
    public async Task FunctionCalling_FullRoundTrip_ProducesAssistantText()
    {
        var (client, m) = RequireSetup();

        var weatherSchema = BinaryData.FromString("""
            {
                "type": "object",
                "properties": {
                    "city": { "type": "string", "description": "The city to look up" }
                },
                "required": ["city"]
            }
            """);

        var initialOptions = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateUserMessageItem("Use get_weather to look up the weather in Seattle, then summarize it.") })
        {
            StoredOutputEnabled = true,
            ToolChoice = OfficialResponses.ResponseToolChoice.CreateRequiredChoice(),
            MaxOutputTokenCount = 256,
            Temperature = 0.0f,
        };
        initialOptions.Tools.Add(OfficialResponses.ResponseTool.CreateFunctionTool(
            functionName: "get_weather",
            functionParameters: weatherSchema,
            strictModeEnabled: true,
            functionDescription: "Get the current weather for a given city."));

        OfficialResponses.ResponseResult firstResponse = await client.CreateResponseAsync(initialOptions).ConfigureAwait(false);

        var functionCall = firstResponse.OutputItems
            .OfType<OfficialResponses.FunctionCallResponseItem>()
            .FirstOrDefault(item => item.FunctionName == "get_weather");
        await Assert.That(functionCall).IsNotNull();

        var argsJson = functionCall!.FunctionArguments?.ToString() ?? "{}";
        string? city = null;
        try
        {
            using var doc = JsonDocument.Parse(argsJson);
            if (doc.RootElement.TryGetProperty("city", out var cityElement))
            {
                city = cityElement.GetString();
            }
        }
        catch (JsonException)
        {
            // small models occasionally emit malformed args; treat as unknown
        }

        var toolOutput = $$$"""{"city": "{{{city ?? "unknown"}}}", "temperatureF": 68, "summary": "partly cloudy"}""";

        var followUpOptions = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateFunctionCallOutputItem(functionCall.CallId, toolOutput) })
        {
            PreviousResponseId = firstResponse.Id,
            StoredOutputEnabled = true,
            MaxOutputTokenCount = 256,
            Temperature = 0.0f,
        };
        followUpOptions.Tools.Add(OfficialResponses.ResponseTool.CreateFunctionTool(
            functionName: "get_weather",
            functionParameters: weatherSchema,
            strictModeEnabled: true,
            functionDescription: "Get the current weather for a given city."));

        OfficialResponses.ResponseResult finalResponse = await client.CreateResponseAsync(followUpOptions).ConfigureAwait(false);

        var finalText = finalResponse.GetOutputText();
        Console.WriteLine($"[FunctionCalling] tool_call_id={functionCall.CallId}, city={city}, final={finalText}");
        await Assert.That(finalText).IsNotNull().And.IsNotEmpty();
    }
}
