// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text;
using System.Threading.Tasks;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;
using Betalgo.Ranul.OpenAI.ObjectModels.SharedModels;

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

        List<ChatMessage> messages =
        [
            // System prompt is setup by GenAI
            new ChatMessage { Role = "user", Content = "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?" }
        ];

        var response = await chatClient.CompleteChatAsync(messages, null).ConfigureAwait(false);

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

        response = await chatClient.CompleteChatAsync(messages, null).ConfigureAwait(false);
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

        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "user", Content = "You are a calculator. Be precise. What is the answer to 7 multiplied by 6?" }
        ];

        var updates = chatClient.CompleteChatStreamingAsync(messages, null, CancellationToken.None).ConfigureAwait(false);

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

        updates = chatClient.CompleteChatStreamingAsync(messages, null, CancellationToken.None).ConfigureAwait(false);
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

    [Test]
    public async Task DirectTool_NoStreaming_Succeeds()
    {
        var chatClient = await model!.GetChatClientAsync();
        await Assert.That(chatClient).IsNotNull();

        chatClient.Settings.MaxTokens = 500;
        chatClient.Settings.Temperature = 0.0f; // for deterministic results
        chatClient.Settings.ToolChoice = ToolChoice.Required; // Force the model to make a tool call

        // Prepare messages and tools
        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "system", Content = "You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question." },
            new ChatMessage { Role = "user", Content = "What is the answer to 7 multiplied by 6?" }
        ];
        List<ToolDefinition> tools =
        [
            new ToolDefinition
            {
                Type = "function",
                Function = new FunctionDefinition()
                {
                    Name = "multiply_numbers",
                    Description = "A tool for multiplying two numbers.",
                    Parameters = new PropertyDefinition()
                    {
                        Type = "object",
                        Properties = new Dictionary<string, PropertyDefinition>()
                        {
                            { "first", new PropertyDefinition() { Type = "integer", Description = "The first number in the operation" } },
                            { "second", new PropertyDefinition() { Type = "integer", Description = "The second number in the operation" } }
                        },
                        Required = ["first", "second"]
                    }
                }
            }
        ];

        // Start the conversation
        var response = await chatClient.CompleteChatAsync(messages, tools).ConfigureAwait(false);
        Console.WriteLine(response.Choices[0].Message.Content);

        // Check that a tool call was generated
        await Assert.That(response).IsNotNull();
        await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Choices.Count).IsEqualTo(1);
        await Assert.That(response.Choices[0].FinishReason).IsEqualTo("tool_calls");

        await Assert.That(response.Choices[0].Message).IsNotNull();
        await Assert.That(response.Choices[0].Message.ToolCalls).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Choices[0].Message.ToolCalls?.Count).IsEqualTo(1);

        // Check the tool call generated by the model
        var toolCall = /*lang=json*/ "[{\"name\": \"multiply_numbers\", \"parameters\": {\"first\": 6, \"second\": 7}}]";
        var expectedResponse = "<tool_call>" + toolCall + "</tool_call>";

        await Assert.That(response.Choices[0].Message.Content).IsEqualTo(expectedResponse);
        await Assert.That(response.Choices[0].Message.ToolCalls?[0].Type).IsEqualTo("function");
        await Assert.That(response.Choices[0].Message.ToolCalls?[0].FunctionCall?.Name).IsEqualTo("multiply_numbers");

        var expectedArguments = /*lang=json*/ "{\r\n  \"first\": 6,\r\n  \"second\": 7\r\n}";
        expectedArguments = OperatingSystemConverter.ToJson(expectedArguments);
        await Assert.That(response.Choices[0].Message.ToolCalls?[0].FunctionCall?.Arguments).IsEqualTo(expectedArguments);

        // Add the response from invoking the tool call to the conversation and check if the model can continue correctly
        var toolCallResponse = "7 x 6 = 42.";
        messages.Add(new ChatMessage { Role = "tool", Content = toolCallResponse });

        // Prompt the model to continue the conversation after the tool call
        messages.Add(new ChatMessage { Role = "system", Content = "Respond only with the answer generated by the tool." });

        // Set tool calling back to auto so that the model can decide whether to call
        // the tool again or continue the conversation based on the new user prompt
        chatClient.Settings.ToolChoice = ToolChoice.Auto;

        // Run the next turn of the conversation
        response = await chatClient.CompleteChatAsync(messages, tools).ConfigureAwait(false);

        // Check that the conversation continued
        await Assert.That(response.Choices[0].Message.Content).IsNotNull();
        await Assert.That(response.Choices[0].Message.Content).Contains("42");
    }

    [Test]
    public async Task DirectTool_Streaming_Succeeds()
    {
        var chatClient = await model!.GetChatClientAsync();
        await Assert.That(chatClient).IsNotNull();

        chatClient.Settings.MaxTokens = 500;
        chatClient.Settings.Temperature = 0.0f; // for deterministic results
        chatClient.Settings.ToolChoice = ToolChoice.Required; // Force the model to make a tool call

        // Prepare messages and tools
        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "system", Content = "You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question." },
            new ChatMessage { Role = "user", Content = "What is the answer to 7 multiplied by 6?" }
        ];
        List<ToolDefinition> tools =
        [
            new ToolDefinition
            {
                Type = "function",
                Function = new FunctionDefinition()
                {
                    Name = "multiply_numbers",
                    Description = "A tool for multiplying two numbers.",
                    Parameters = new PropertyDefinition()
                    {
                        Type = "object",
                        Properties = new Dictionary<string, PropertyDefinition>()
                        {
                            { "first", new PropertyDefinition() { Type = "integer", Description = "The first number in the operation" } },
                            { "second", new PropertyDefinition() { Type = "integer", Description = "The second number in the operation" } }
                        },
                        Required = ["first", "second"]
                    }
                }
            }
        ];

        // Start the conversation
        var updates = chatClient.CompleteChatStreamingAsync(messages, tools, CancellationToken.None).ConfigureAwait(false);

        // Check that each response chunk contains the expected information
        StringBuilder responseMessage = new();
        var numTokens = 0;
        ChatCompletionCreateResponse? toolCallResponse = null;
        await foreach (var response in updates)
        {
            await Assert.That(response).IsNotNull();
            await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
            await Assert.That(response.Choices[0].Message).IsNotNull();
            var content = response.Choices[0].Message.Content;
            await Assert.That(content).IsNotNull();
            Console.WriteLine($"Content in streaming: {content}, Finish reason: {response.Choices[0].FinishReason}");
            if (!string.IsNullOrEmpty(content))
            {
                responseMessage.Append(content);
                numTokens += 1;
            }
            if (response.Choices[0].FinishReason == "tool_calls")
            {
                toolCallResponse = response;
            }
        }

        // Check that the full response contains the expected tool call and that the tool call information is correct
        var fullResponse = responseMessage.ToString();
        Console.WriteLine(fullResponse);
        var toolCall = /*lang=json*/ "[{\"name\": \"multiply_numbers\", \"parameters\": {\"first\": 6, \"second\": 7}}]";
        var expectedResponse = "<tool_call>" + toolCall + "</tool_call>";
        await Assert.That(numTokens).IsLessThanOrEqualTo(chatClient.Settings.MaxTokens.Value);

        await Assert.That(fullResponse).IsNotNull();
        await Assert.That(fullResponse).IsEqualTo(expectedResponse);
        await Assert.That(toolCallResponse?.Choices.Count).IsEqualTo(1);
        await Assert.That(toolCallResponse?.Choices[0].FinishReason).IsEqualTo("tool_calls");
        await Assert.That(toolCallResponse?.Choices[0].Message.ToolCalls).IsNotNull();
        await Assert.That(toolCallResponse?.Choices[0].Message.ToolCalls?.Count).IsEqualTo(1);
        await Assert.That(toolCallResponse?.Choices[0].Message.ToolCalls?[0].Type).IsEqualTo("function");
        await Assert.That(toolCallResponse?.Choices[0].Message.ToolCalls?[0].FunctionCall?.Name).IsEqualTo("multiply_numbers");

        var expectedArguments = /*lang=json*/ "{\r\n  \"first\": 6,\r\n  \"second\": 7\r\n}";
        expectedArguments = OperatingSystemConverter.ToJson(expectedArguments);
        await Assert.That(toolCallResponse?.Choices[0].Message.ToolCalls?[0].FunctionCall?.Arguments).IsEqualTo(expectedArguments);

        // Add the response from invoking the tool call to the conversation and check if the model can continue correctly
        var toolResponse = "7 x 6 = 42.";
        messages.Add(new ChatMessage { Role = "tool", Content = toolResponse });

        // Prompt the model to continue the conversation after the tool call
        messages.Add(new ChatMessage { Role = "system", Content = "Respond only with the answer generated by the tool." });

        // Set tool calling back to auto so that the model can decide whether to call
        // the tool again or continue the conversation based on the new user prompt
        chatClient.Settings.ToolChoice = ToolChoice.Auto;

        // Run the next turn of the conversation
        updates = chatClient.CompleteChatStreamingAsync(messages, tools, CancellationToken.None).ConfigureAwait(false);
        responseMessage.Clear();
        await foreach (var response in updates)
        {
            await Assert.That(response).IsNotNull();
            await Assert.That(response.Choices).IsNotNull().And.IsNotEmpty();
            await Assert.That(response.Choices[0].Message).IsNotNull();
            var content = response.Choices[0].Message.Content;
            await Assert.That(content).IsNotNull();
            Console.WriteLine($"Content in streaming: {content}, Finish reason: {response.Choices[0].FinishReason}");
            if (!string.IsNullOrEmpty(content))
            {
                responseMessage.Append(content);
            }
        }

        // Check that the conversation continued
        fullResponse = responseMessage.ToString();
        Console.WriteLine(fullResponse);
        await Assert.That(fullResponse).Contains("42");
    }
}
