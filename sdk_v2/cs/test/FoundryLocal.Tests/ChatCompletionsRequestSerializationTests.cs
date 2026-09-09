// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Collections.Generic;
using System.Text.Json;
using System.Threading.Tasks;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

using Microsoft.AI.Foundry.Local.OpenAI;

/// <summary>
/// Wire-format tests for the OpenAI Chat Completions request the SDK hands to native. They need
/// neither the native library nor a downloaded model, so they run on every target framework and
/// guard the tool-calling contract that <see cref="OpenAIChatCompletionsTests"/> can only reach
/// with a live model.
///
/// The payload is self-contained: native correlates every tool result against an empty transcript,
/// so a request that answers a tool call must carry both the assistant "tool_calls" message that
/// issued the call and the matching "tool_call_id" on the result. The SDK passes the caller's
/// messages through untouched, which is exactly what these tests pin down.
/// </summary>
internal sealed class ChatCompletionsRequestSerializationTests
{
    private const string CallId = "call_abc123";

    private const string Arguments = /*lang=json*/ "{\"first\":7,\"second\":6}";

    [Test]
    public async Task ToolResultMessage_CarriesToolCallId()
    {
        var json = BuildRequestJson(
        [
            new ChatMessage { Role = "user", Content = "What is the answer to 7 multiplied by 6?" },
            AssistantToolCallMessage(),
            new ChatMessage { Role = "tool", ToolCallId = CallId, Content = "7 x 6 = 42." }
        ]);

        using var document = JsonDocument.Parse(json);
        var messages = document.RootElement.GetProperty("messages");

        await Assert.That(messages.GetArrayLength()).IsEqualTo(3);

        // The assistant turn that issued the call has to precede the result that answers it.
        await Assert.That(messages[1].GetProperty("role").GetString()).IsEqualTo("assistant");

        var toolMessage = messages[2];
        await Assert.That(toolMessage.GetProperty("role").GetString()).IsEqualTo("tool");
        await Assert.That(toolMessage.GetProperty("tool_call_id").GetString()).IsEqualTo(CallId);
        await Assert.That(toolMessage.GetProperty("content").GetString()).IsEqualTo("7 x 6 = 42.");
    }

    [Test]
    public async Task AssistantToolCallMessage_CarriesCallIdNameAndArguments()
    {
        var json = BuildRequestJson([AssistantToolCallMessage()]);

        using var document = JsonDocument.Parse(json);
        var assistant = document.RootElement.GetProperty("messages")[0];

        await Assert.That(assistant.GetProperty("role").GetString()).IsEqualTo("assistant");

        var toolCalls = assistant.GetProperty("tool_calls");
        await Assert.That(toolCalls.GetArrayLength()).IsEqualTo(1);
        await Assert.That(toolCalls[0].GetProperty("id").GetString()).IsEqualTo(CallId);
        await Assert.That(toolCalls[0].GetProperty("type").GetString()).IsEqualTo("function");

        var function = toolCalls[0].GetProperty("function");
        await Assert.That(function.GetProperty("name").GetString()).IsEqualTo("multiply_numbers");
        await Assert.That(function.GetProperty("arguments").GetString()).IsEqualTo(Arguments);

        // An assistant turn that issues a call has no visible text of its own, so "content" is
        // omitted rather than written as null: the replay reproduces the content-free turn the
        // model generated instead of feeding its tool-call marker text back into the conversation.
        await Assert.That(assistant.TryGetProperty("content", out _)).IsFalse();
    }

    [Test]
    public async Task ToolResultMessage_WithoutToolCallId_OmitsTheProperty()
    {
        // Pins the shape native rejects with "tool result requires a non-empty call id": the SDK does
        // not fabricate a call id, so omitting ToolCallId omits the property rather than sending "".
        var json = BuildRequestJson([new ChatMessage { Role = "tool", Content = "7 x 6 = 42." }]);

        using var document = JsonDocument.Parse(json);
        var toolMessage = document.RootElement.GetProperty("messages")[0];

        await Assert.That(toolMessage.GetProperty("role").GetString()).IsEqualTo("tool");
        await Assert.That(toolMessage.TryGetProperty("tool_call_id", out _)).IsFalse();
    }

    private static ChatMessage AssistantToolCallMessage()
    {
        return new ChatMessage
        {
            Role = "assistant",
            ToolCalls =
            [
                new ToolCall
                {
                    Id = CallId,
                    Type = "function",
                    FunctionCall = new FunctionCall { Name = "multiply_numbers", Arguments = Arguments }
                }
            ]
        };
    }

    private static string BuildRequestJson(IEnumerable<ChatMessage> messages)
    {
#pragma warning disable CS0618 // FromUserInput takes the (obsolete) OpenAIChatClient.ChatSettings by design.
        var settings = new OpenAIChatClient.ChatSettings();
#pragma warning restore CS0618

        return ChatCompletionCreateRequestExtended
            .FromUserInput("test-model", messages, tools: null, settings, stream: false)
            .ToJson();
    }
}
