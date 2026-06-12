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
/// Mirrors <c>sdk/js/test/openai/responsesWebService.test.ts</c> from the JS PR:
/// - Skips when running in CI.
/// - Skips when <c>qwen2.5-0.5b</c> is not in the local cache.
/// - Covers non-streaming, streaming, and a full function-calling round-trip.
/// </summary>
internal sealed class ResponsesIntegrationTests
{
    private const string ModelAlias = "qwen2.5-0.5b";
    private const string ModelVariant = "qwen2.5-0.5b-instruct-generic-cpu:4";

    private static IModel? model;
    private static OfficialOpenAI.OpenAIClient? openAiClient;
    private static OfficialResponses.ResponsesClient? responses;
    private static string? skipReason;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance; // initialized by Utils

        var catalog = await manager.GetCatalogAsync();
        var modelVariant = await catalog.GetModelVariantAsync(ModelVariant).ConfigureAwait(false);
        if (modelVariant is null)
        {
            skipReason = $"Model variant '{ModelVariant}' is not in the catalog.";
            return;
        }

        if (!await modelVariant.IsCachedAsync().ConfigureAwait(false))
        {
            skipReason = $"Model '{ModelAlias}' is not cached locally; pre-cache via the SDK to enable these tests.";
            return;
        }

        await modelVariant.LoadAsync().ConfigureAwait(false);
        await Assert.That(await modelVariant.IsLoadedAsync()).IsTrue();
        model = modelVariant;

        await manager.StartWebServiceAsync().ConfigureAwait(false);
        await Assert.That(manager.Urls).IsNotNull().And.IsNotEmpty();

        var endpoint = new Uri(manager.Urls![0].TrimEnd('/') + "/v1");
        openAiClient = new OfficialOpenAI.OpenAIClient(
            new ApiKeyCredential("notneeded"),
            new OfficialOpenAI.OpenAIClientOptions { Endpoint = endpoint });
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
        if (skipReason is not null || responses is null || model is null)
        {
            Skip.Test(skipReason ?? "Responses integration setup did not complete.");
        }

        return (responses!, model!);
    }

    [Test]
    public async Task NonStreaming_SimplePrompt_ReturnsCompletedResponseWithText()
    {
        var (client, m) = RequireSetup();

        var options = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateUserMessageItem("What is 2 + 2? Answer with just the number.") })
        {
            Temperature = 0.0f,
            MaxOutputTokenCount = 64,
            StoredOutputEnabled = false,
        };

        OfficialResponses.ResponseResult response = await client.CreateResponseAsync(options).ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Status).IsEqualTo(OfficialResponses.ResponseStatus.Completed);

        var text = response.GetOutputText();
        Console.WriteLine($"[NonStreaming] {text}");
        await Assert.That(text).IsNotNull().And.IsNotEmpty();
    }

    [Test]
    public async Task Streaming_EmitsCreatedDeltaAndCompletedEvents()
    {
        var (client, m) = RequireSetup();

        var options = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateUserMessageItem("Count from 1 to 3.") })
        {
            Temperature = 0.0f,
            MaxOutputTokenCount = 64,
            StoredOutputEnabled = false,
            StreamingEnabled = true,
        };

        var sawCreated = false;
        var sawTextDelta = false;
        var sawCompleted = false;
        var aggregate = new StringBuilder();

        await foreach (OfficialResponses.StreamingResponseUpdate update in client.CreateResponseStreamingAsync(options).ConfigureAwait(false))
        {
            switch (update)
            {
                case OfficialResponses.StreamingResponseCreatedUpdate:
                    sawCreated = true;
                    break;
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
        await Assert.That(sawCreated).IsTrue();
        await Assert.That(sawTextDelta).IsTrue();
        await Assert.That(sawCompleted).IsTrue();
    }

    [Test]
    public async Task FunctionCalling_FullRoundTrip_ProducesAssistantText()
    {
        var (client, m) = RequireSetup();

        var emptyParamsSchema = BinaryData.FromString("""
            {
                "type": "object",
                "properties": {},
                "additionalProperties": false
            }
            """);

        OfficialResponses.ResponseTool getWeatherTool = OfficialResponses.ResponseTool.CreateFunctionTool(
            functionName: "get_weather",
            functionParameters: emptyParamsSchema,
            strictModeEnabled: true,
            functionDescription: "Get the current weather. This test always returns Seattle weather.");

        var initialOptions = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateUserMessageItem("Use the get_weather tool and then answer with the weather.") })
        {
            ToolChoice = OfficialResponses.ResponseToolChoice.CreateRequiredChoice(),
            Temperature = 0.0f,
            MaxOutputTokenCount = 64,
            StoredOutputEnabled = true,
        };
        initialOptions.Tools.Add(getWeatherTool);

        OfficialResponses.ResponseResult toolResponse = await client.CreateResponseAsync(initialOptions).ConfigureAwait(false);

        var functionCall = toolResponse.OutputItems
            .OfType<OfficialResponses.FunctionCallResponseItem>()
            .FirstOrDefault(item => item.FunctionName == "get_weather");
        await Assert.That(functionCall).IsNotNull();
        await Assert.That(functionCall!.CallId).IsNotNull().And.IsNotEmpty();

        const string toolOutput = """{"location": "Seattle", "weather": "72 degrees F and sunny"}""";

        var followUpOptions = new OfficialResponses.CreateResponseOptions(
            m.Id,
            new[] { OfficialResponses.ResponseItem.CreateFunctionCallOutputItem(functionCall.CallId, toolOutput) })
        {
            PreviousResponseId = toolResponse.Id,
            Temperature = 0.0f,
            MaxOutputTokenCount = 64,
            StoredOutputEnabled = false,
        };
        followUpOptions.Tools.Add(getWeatherTool);

        OfficialResponses.ResponseResult finalResponse = await client.CreateResponseAsync(followUpOptions).ConfigureAwait(false);

        await Assert.That(finalResponse.Status).IsEqualTo(OfficialResponses.ResponseStatus.Completed);

        var finalText = finalResponse.GetOutputText();
        Console.WriteLine($"[FunctionCalling] tool_call_id={functionCall.CallId} final={finalText}");
        await Assert.That(finalText).IsNotNull().And.IsNotEmpty();
    }
}
