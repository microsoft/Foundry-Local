// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.OpenAI.Responses;

/// <summary>
/// End-to-end integration tests for <see cref="OpenAIResponsesClient"/>. Requires the Foundry Local
/// service to be able to load and serve the configured model. Runs are category-tagged so they can
/// be skipped in CI environments that don't have the model cache available.
/// </summary>
internal sealed class ResponsesIntegrationTests
{
    private const string ModelId = "qwen2.5-0.5b-instruct-generic-cpu:4";

    private static IModel? model;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance;
        var catalog = await manager.GetCatalogAsync();

        var m = await catalog.GetModelVariantAsync(ModelId).ConfigureAwait(false);
        await Assert.That(m).IsNotNull();

        await m!.LoadAsync().ConfigureAwait(false);
        await Assert.That(await m.IsLoadedAsync()).IsTrue();

        model = m;
    }

    [Test]
    public async Task NonStreaming_SimpleString()
    {
        using var client = await model!.GetResponsesClientAsync();
        var response = await client.CreateAsync("Say the single word: ready").ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.OutputText).IsNotNull().And.IsNotEmpty();
        Console.WriteLine($"[NonStreaming_SimpleString] {response.OutputText}");
    }

    [Test]
    public async Task NonStreaming_WithOptions()
    {
        using var client = await model!.GetResponsesClientAsync();
        var response = await client.CreateAsync(
            "What is 7 * 6? Respond only with the number.",
            r =>
            {
                r.MaxOutputTokens = 32;
                r.Temperature = 0.0f;
                r.Instructions = "You are a calculator. Respond precisely.";
            }).ConfigureAwait(false);

        await Assert.That(response.OutputText).Contains("42");
    }

    [Test]
    public async Task NonStreaming_StructuredInput()
    {
        using var client = await model!.GetResponsesClientAsync();

        var items = new List<ResponseItem>
        {
            new MessageItem
            {
                Role = MessageRole.User,
                Content = MessageContent.FromParts(new InputTextContent { Text = "Say: hello" }),
            },
        };

        var response = await client.CreateAsync(items, r => r.MaxOutputTokens = 16).ConfigureAwait(false);
        await Assert.That(response.OutputText).IsNotEmpty();
    }

    [Test]
    public async Task MultiTurn_PreviousResponseId()
    {
        using var client = await model!.GetResponsesClientAsync();
        var first = await client.CreateAsync(
            "Remember the number 17.",
            r => { r.MaxOutputTokens = 32; r.Store = true; }).ConfigureAwait(false);

        await Assert.That(first.Id).IsNotNull().And.IsNotEmpty();

        var second = await client.CreateAsync(
            "What was the number?",
            r =>
            {
                r.PreviousResponseId = first.Id;
                r.MaxOutputTokens = 32;
                r.Temperature = 0.0f;
            }).ConfigureAwait(false);

        // The small qwen model may or may not recall the exact number — what we really
        // validate is that the multi-turn wiring (previous_response_id) produces a response
        // that continues the conversation. Don't assert on model content.
        await Assert.That(second.Id).IsNotNull().And.IsNotEmpty();
        await Assert.That(second.OutputText).IsNotEmpty();
    }

    [Test]
    public async Task Streaming_ReceivesDeltaEvents()
    {
        using var client = await model!.GetResponsesClientAsync();

        var sawDelta = false;
        var sawCompleted = false;
        var aggregate = new StringBuilder();

        await foreach (var evt in client.CreateStreamingAsync(
            "Count from 1 to 3.",
            r => { r.MaxOutputTokens = 64; r.Temperature = 0.0f; }))
        {
            if (evt is OutputTextDeltaEvent delta)
            {
                sawDelta = true;
                aggregate.Append(delta.Delta);
            }
            else if (evt is ResponseCompletedEvent)
            {
                sawCompleted = true;
            }
        }

        await Assert.That(sawDelta).IsTrue();
        await Assert.That(sawCompleted).IsTrue();
        Console.WriteLine($"[Streaming] aggregated: {aggregate}");
    }

    [Test]
    public async Task GetStoredResponse()
    {
        using var client = await model!.GetResponsesClientAsync();
        var created = await client.CreateAsync("Say: stored", r => { r.Store = true; r.MaxOutputTokens = 16; });
        var fetched = await client.GetAsync(created.Id);
        await Assert.That(fetched.Id).IsEqualTo(created.Id);
    }

    [Test]
    public async Task DeleteResponse()
    {
        using var client = await model!.GetResponsesClientAsync();
        var created = await client.CreateAsync("Say: delete-me", r => { r.Store = true; r.MaxOutputTokens = 16; });
        var result = await client.DeleteAsync(created.Id);
        await Assert.That(result.Deleted).IsTrue();
    }

    [Test]
    public async Task ListResponses()
    {
        using var client = await model!.GetResponsesClientAsync();
        _ = await client.CreateAsync("Hello", r => { r.Store = true; r.MaxOutputTokens = 8; });
        var list = await client.ListAsync();
        await Assert.That(list).IsNotNull();
        await Assert.That(list.Data).IsNotNull();
    }

    [Test]
    public async Task GetInputItems()
    {
        using var client = await model!.GetResponsesClientAsync();
        var created = await client.CreateAsync("Hi", r => { r.Store = true; r.MaxOutputTokens = 8; });
        var items = await client.GetInputItemsAsync(created.Id);
        await Assert.That(items).IsNotNull();
        await Assert.That(items.Data).IsNotNull();
    }
}
