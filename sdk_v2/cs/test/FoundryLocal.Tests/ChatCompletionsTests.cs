// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text;
using System.Threading.Tasks;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

internal sealed class ChatCompletionsTests
{
    private static Model? model;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance; // initialized by Utils
        var catalog = await manager.GetCatalogAsync();
        
        // Load the specific cached model variant directly
        var modelVariant = await catalog.GetModelVariantAsync("qwen2.5-0.5b-instruct-generic-cpu:4").ConfigureAwait(false);
        await Assert.That(modelVariant).IsNotNull();
        
        var model = new Model(modelVariant!, manager.Logger);
        await model.LoadAsync().ConfigureAwait(false);
        await Assert.That(await model.IsLoadedAsync()).IsTrue();

        ChatCompletionsTests.model = model;
    }

    [Test]
    public async Task DirectChat_NoStreaming_Succeeds()
    {
        var chatClient = await model!.GetChatClientAsync();
        await Assert.That(chatClient).IsNotNull();

        chatClient.Settings.MaxTokens = 500;
        chatClient.Settings.Temperature = 0.0f; // for deterministic results

        List<ChatMessage> messages = new()
        {
            // System prompt is setup by GenAI
            new ChatMessage { Role = "user", Content = "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?" }
        };

        var response = await chatClient.CompleteChatAsync(messages).ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
        var message = response.Choices[0].Message;
        await Assert.That(message).IsNotNull();
        await Assert.That(message.Role).IsEqualTo("assistant");
        await Assert.That(message.Content).IsNotNull();
        await Assert.That(message.Content).Contains("42");
        Console.WriteLine($"Response: {message.Content}");

        messages.Add(new ChatMessage { Role = "assistant", Content = message.Content });

        messages.Add(new ChatMessage
        {
            Role = "user",
            Content = "Is the answer a real number?"
        });

        response = await chatClient.CompleteChatAsync(messages).ConfigureAwait(false);
        message = response.Choices[0].Message;
        await Assert.That(message.Content).IsNotNull();
        await Assert.That(message.Content).Contains("Yes");
    }

    [Test]
    public async Task DirectChat_Streaming_Succeeds()
    {
        var chatClient = await model!.GetChatClientAsync();
        await Assert.That(chatClient).IsNotNull();
        
        chatClient.Settings.MaxTokens = 500;
        chatClient.Settings.Temperature = 0.0f; // for deterministic results

        List<ChatMessage> messages = new()
        {
            new ChatMessage { Role = "user", Content = "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?" }
        };

        var updates = chatClient.CompleteChatStreamingAsync(messages, CancellationToken.None).ConfigureAwait(false);

        StringBuilder responseMessage = new();
        await foreach (var response in updates)
        {
            await Assert.That(response).IsNotNull();
            await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
            var message = response.Choices[0].Message;
            await Assert.That(message).IsNotNull();
            await Assert.That(message.Role).IsEqualTo("assistant");
            await Assert.That(message.Content).IsNotNull();
            responseMessage.Append(message.Content);
        }

        var fullResponse = responseMessage.ToString();
        Console.WriteLine(fullResponse);
        await Assert.That(fullResponse).Contains("42");

        messages.Add(new ChatMessage { Role = "assistant", Content = fullResponse });
        messages.Add(new ChatMessage
        {
            Role = "user",
            Content = "Add 25 to the previous answer. Think hard to be sure of the answer."
        });

        updates = chatClient.CompleteChatStreamingAsync(messages, CancellationToken.None).ConfigureAwait(false);
        responseMessage.Clear();
        await foreach (var response in updates)
        {
            await Assert.That(response).IsNotNull();
            await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
            var message = response.Choices[0].Message;
            await Assert.That(message).IsNotNull();
            await Assert.That(message.Role).IsEqualTo("assistant");
            await Assert.That(message.Content).IsNotNull();
            responseMessage.Append(message.Content);
        }

        fullResponse = responseMessage.ToString();
        Console.WriteLine(fullResponse);
        await Assert.That(fullResponse).Contains("67");
    }
}
