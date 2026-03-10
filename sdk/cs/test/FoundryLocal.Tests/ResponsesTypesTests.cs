// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text.Json;

using Microsoft.AI.Foundry.Local.OpenAI;

/// <summary>
/// Unit tests for Responses API types, JSON converters, and serialization.
/// These tests exercise the DTOs and custom converters without requiring
/// the Foundry Local runtime.
/// </summary>
internal sealed class ResponsesTypesTests
{
    // ========================================================================
    // ResponseInput serialization (string vs array)
    // ========================================================================

    [Test]
    public async Task ResponseInput_StringInput_SerializesAsString()
    {
        var input = new ResponseInput { Text = "Hello, world!" };
        var json = JsonSerializer.Serialize(input, ResponsesJsonContext.Default.ResponseInput);

        await Assert.That(json).IsEqualTo("\"Hello, world!\"");
    }

    [Test]
    public async Task ResponseInput_StringInput_DeserializesFromString()
    {
        var json = "\"What is the capital of France?\"";
        var input = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseInput);

        await Assert.That(input).IsNotNull();
        await Assert.That(input!.Text).IsEqualTo("What is the capital of France?");
        await Assert.That(input.Items).IsNull();
    }

    [Test]
    public async Task ResponseInput_ItemsInput_SerializesAsArray()
    {
        var input = new ResponseInput
        {
            Items =
            [
                new ResponseMessageItem
                {
                    Role = "user",
                    Content = new ResponseMessageContent { Text = "Hi" }
                }
            ]
        };

        var json = JsonSerializer.Serialize(input, ResponsesJsonContext.Default.ResponseInput);
        await Assert.That(json).Contains("\"type\":\"message\"");
        await Assert.That(json).Contains("\"role\":\"user\"");
    }

    [Test]
    public async Task ResponseInput_ItemsInput_DeserializesFromArray()
    {
        var json = """[{"type":"message","role":"user","content":"Hi there"}]""";
        var input = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseInput);

        await Assert.That(input).IsNotNull();
        await Assert.That(input!.Text).IsNull();
        await Assert.That(input.Items).IsNotNull().And.HasCount().EqualTo(1);

        var msg = input.Items![0] as ResponseMessageItem;
        await Assert.That(msg).IsNotNull();
        await Assert.That(msg!.Role).IsEqualTo("user");
        await Assert.That(msg.Content.Text).IsEqualTo("Hi there");
    }

    [Test]
    public async Task ResponseInput_ImplicitConversionFromString()
    {
        ResponseInput input = "Hello";
        await Assert.That(input.Text).IsEqualTo("Hello");
        await Assert.That(input.Items).IsNull();
    }

    // ========================================================================
    // ResponseToolChoice serialization (string vs object)
    // These are tested via ResponseCreateRequest since the converter is
    // applied at property level, not on the type directly.
    // ========================================================================

    [Test]
    public async Task ToolChoice_StringValue_SerializesViaRequest()
    {
        var request = new ResponseCreateRequest
        {
            Model = "phi-4",
            ToolChoice = ResponseToolChoice.Auto
        };

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);
        await Assert.That(json).Contains("\"tool_choice\":\"auto\"");
    }

    [Test]
    public async Task ToolChoice_StringValue_DeserializesViaResponse()
    {
        var json = """{"id":"r1","object":"response","status":"completed","model":"phi-4","output":[],"tools":[],"tool_choice":"required","parallel_tool_calls":false,"top_p":1,"temperature":0.7,"presence_penalty":0,"frequency_penalty":0,"store":false,"created_at":0}""";
        var response = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseObject);

        await Assert.That(response).IsNotNull();
        await Assert.That(response!.ToolChoice).IsNotNull();
        await Assert.That(response.ToolChoice!.Value).IsEqualTo("required");
        await Assert.That(response.ToolChoice.Specific).IsNull();
    }

    [Test]
    public async Task ToolChoice_SpecificFunction_SerializesViaRequest()
    {
        var request = new ResponseCreateRequest
        {
            Model = "phi-4",
            ToolChoice = new ResponseToolChoice
            {
                Specific = new ResponseSpecificToolChoice { Name = "get_weather" }
            }
        };

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);
        await Assert.That(json).Contains("\"type\":\"function\"");
        await Assert.That(json).Contains("\"name\":\"get_weather\"");
    }

    [Test]
    public async Task ToolChoice_SpecificFunction_DeserializesViaResponse()
    {
        var json = """{"id":"r1","object":"response","status":"completed","model":"phi-4","output":[],"tools":[],"tool_choice":{"type":"function","name":"get_weather"},"parallel_tool_calls":false,"top_p":1,"temperature":0.7,"presence_penalty":0,"frequency_penalty":0,"store":false,"created_at":0}""";
        var response = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseObject);

        await Assert.That(response).IsNotNull();
        await Assert.That(response!.ToolChoice).IsNotNull();
        await Assert.That(response.ToolChoice!.Value).IsNull();
        await Assert.That(response.ToolChoice.Specific).IsNotNull();
        await Assert.That(response.ToolChoice.Specific!.Name).IsEqualTo("get_weather");
    }

    [Test]
    public async Task ToolChoice_ImplicitConversionFromString()
    {
        ResponseToolChoice choice = "none";
        await Assert.That(choice.Value).IsEqualTo("none");
    }

    // ========================================================================
    // ResponseMessageContent serialization (string vs parts array)
    // ========================================================================

    [Test]
    public async Task MessageContent_String_SerializesAsString()
    {
        var content = new ResponseMessageContent { Text = "Hello" };
        var json = JsonSerializer.Serialize(content, ResponsesJsonContext.Default.ResponseMessageContent);

        await Assert.That(json).IsEqualTo("\"Hello\"");
    }

    [Test]
    public async Task MessageContent_String_DeserializesFromString()
    {
        var json = "\"Hello, world!\"";
        var content = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseMessageContent);

        await Assert.That(content).IsNotNull();
        await Assert.That(content!.Text).IsEqualTo("Hello, world!");
        await Assert.That(content.Parts).IsNull();
    }

    [Test]
    public async Task MessageContent_Parts_DeserializesFromArray()
    {
        var json = """[{"type":"output_text","text":"Generated text"}]""";
        var content = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseMessageContent);

        await Assert.That(content).IsNotNull();
        await Assert.That(content!.Text).IsNull();
        await Assert.That(content.Parts).IsNotNull().And.HasCount().EqualTo(1);

        var textPart = content.Parts![0] as ResponseOutputTextContent;
        await Assert.That(textPart).IsNotNull();
        await Assert.That(textPart!.Text).IsEqualTo("Generated text");
    }

    // ========================================================================
    // ResponseObject.OutputText property
    // ========================================================================

    [Test]
    public async Task OutputText_ReturnsAssistantMessageText()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new ResponseMessageItem
                {
                    Role = "assistant",
                    Content = new ResponseMessageContent { Text = "The answer is 42." }
                }
            ]
        };

        await Assert.That(response.OutputText).IsEqualTo("The answer is 42.");
    }

    [Test]
    public async Task OutputText_ReturnsEmptyWhenNoAssistantMessage()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new ResponseMessageItem
                {
                    Role = "user",
                    Content = new ResponseMessageContent { Text = "A question" }
                }
            ]
        };

        await Assert.That(response.OutputText).IsEqualTo(string.Empty);
    }

    [Test]
    public async Task OutputText_ReturnsEmptyWhenOutputIsEmpty()
    {
        var response = new ResponseObject { Output = [] };
        await Assert.That(response.OutputText).IsEqualTo(string.Empty);
    }

    [Test]
    public async Task OutputText_ConcatenatesTextParts()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new ResponseMessageItem
                {
                    Role = "assistant",
                    Content = new ResponseMessageContent
                    {
                        Parts =
                        [
                            new ResponseOutputTextContent { Text = "Part one. " },
                            new ResponseOutputTextContent { Text = "Part two." }
                        ]
                    }
                }
            ]
        };

        await Assert.That(response.OutputText).IsEqualTo("Part one. Part two.");
    }

    [Test]
    public async Task OutputText_IgnoresRefusalParts()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new ResponseMessageItem
                {
                    Role = "assistant",
                    Content = new ResponseMessageContent
                    {
                        Parts =
                        [
                            new ResponseRefusalContent { Refusal = "I can't do that" },
                            new ResponseOutputTextContent { Text = "But here is something else." }
                        ]
                    }
                }
            ]
        };

        await Assert.That(response.OutputText).IsEqualTo("But here is something else.");
    }

    // ========================================================================
    // ResponseItem polymorphic serialization
    // ========================================================================

    [Test]
    public async Task ResponseItem_MessageItem_RoundTrips()
    {
        var item = new ResponseMessageItem
        {
            Id = "msg_001",
            Role = "assistant",
            Status = "completed",
            Content = new ResponseMessageContent { Text = "Hello" }
        };

        var json = JsonSerializer.Serialize<ResponseItem>(item, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(json).Contains("\"type\":\"message\"");

        var deserialized = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(deserialized).IsTypeOf<ResponseMessageItem>();
        var msg = (ResponseMessageItem)deserialized!;
        await Assert.That(msg.Id).IsEqualTo("msg_001");
        await Assert.That(msg.Role).IsEqualTo("assistant");
    }

    [Test]
    public async Task ResponseItem_FunctionCall_RoundTrips()
    {
        var item = new ResponseFunctionCallItem
        {
            Id = "fc_001",
            CallId = "call_123",
            Name = "get_weather",
            Arguments = """{"city":"Seattle"}"""
        };

        var json = JsonSerializer.Serialize<ResponseItem>(item, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(json).Contains("\"type\":\"function_call\"");
        await Assert.That(json).Contains("\"call_id\":\"call_123\"");

        var deserialized = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(deserialized).IsTypeOf<ResponseFunctionCallItem>();
        var fc = (ResponseFunctionCallItem)deserialized!;
        await Assert.That(fc.Name).IsEqualTo("get_weather");
    }

    [Test]
    public async Task ResponseItem_FunctionCallOutput_RoundTrips()
    {
        var item = new ResponseFunctionCallOutputItem
        {
            CallId = "call_123",
            Output = """{"temp":72}"""
        };

        var json = JsonSerializer.Serialize<ResponseItem>(item, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(json).Contains("\"type\":\"function_call_output\"");

        var deserialized = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseItem);
        await Assert.That(deserialized).IsTypeOf<ResponseFunctionCallOutputItem>();
    }

    // ========================================================================
    // ResponseCreateRequest serialization
    // ========================================================================

    [Test]
    public async Task CreateRequest_OmitsNullFields()
    {
        var request = new ResponseCreateRequest
        {
            Model = "phi-4",
            Input = new ResponseInput { Text = "Hello" },
            Stream = false
        };

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);

        await Assert.That(json).Contains("\"model\":\"phi-4\"");
        await Assert.That(json).Contains("\"input\":\"Hello\"");
        await Assert.That(json).DoesNotContain("\"instructions\"");
        await Assert.That(json).DoesNotContain("\"tools\"");
        await Assert.That(json).DoesNotContain("\"temperature\"");
    }

    [Test]
    public async Task CreateRequest_WithTools_Serializes()
    {
        var request = new ResponseCreateRequest
        {
            Model = "phi-4",
            Input = new ResponseInput { Text = "What's the weather?" },
            Tools =
            [
                new ResponseFunctionTool
                {
                    Name = "get_weather",
                    Description = "Get weather for a city",
                    Strict = true
                }
            ],
            ToolChoice = ResponseToolChoice.Auto
        };

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);

        await Assert.That(json).Contains("\"name\":\"get_weather\"");
        await Assert.That(json).Contains("\"tool_choice\":\"auto\"");
    }

    // ========================================================================
    // Streaming event deserialization
    // ========================================================================

    [Test]
    public async Task StreamingEvent_TextDelta_Deserializes()
    {
        var json = """{"type":"response.output_text.delta","sequence_number":5,"delta":"Hello","output_index":0,"content_index":0}""";
        var evt = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseStreamingEvent);

        await Assert.That(evt).IsNotNull();
        await Assert.That(evt!.Type).IsEqualTo("response.output_text.delta");
        await Assert.That(evt.Delta).IsEqualTo("Hello");
        await Assert.That(evt.SequenceNumber).IsEqualTo(5);
    }

    [Test]
    public async Task StreamingEvent_ResponseCompleted_Deserializes()
    {
        var json = """{"type":"response.completed","sequence_number":10,"response":{"id":"resp_001","object":"response","status":"completed","model":"phi-4","output":[],"created_at":0,"tools":[]}}""";
        var evt = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseStreamingEvent);

        await Assert.That(evt).IsNotNull();
        await Assert.That(evt!.Type).IsEqualTo("response.completed");
        await Assert.That(evt.Response).IsNotNull();
        await Assert.That(evt.Response!.Id).IsEqualTo("resp_001");
        await Assert.That(evt.Response.Status).IsEqualTo("completed");
    }

    // ========================================================================
    // Full response deserialization (simulating server response)
    // ========================================================================

    [Test]
    public async Task ResponseObject_FullJson_Deserializes()
    {
        var json = """
        {
            "id": "resp_abc123",
            "object": "response",
            "created_at": 1710000000,
            "completed_at": 1710000001,
            "status": "completed",
            "model": "phi-4",
            "output": [
                {
                    "type": "message",
                    "id": "msg_001",
                    "role": "assistant",
                    "status": "completed",
                    "content": [
                        {"type": "output_text", "text": "42 is the answer."}
                    ]
                }
            ],
            "tools": [],
            "parallel_tool_calls": false,
            "temperature": 0.7,
            "top_p": 1.0,
            "presence_penalty": 0.0,
            "frequency_penalty": 0.0,
            "store": false,
            "usage": {
                "input_tokens": 10,
                "output_tokens": 5,
                "total_tokens": 15
            }
        }
        """;

        var response = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseObject);

        await Assert.That(response).IsNotNull();
        await Assert.That(response!.Id).IsEqualTo("resp_abc123");
        await Assert.That(response.Status).IsEqualTo("completed");
        await Assert.That(response.OutputText).IsEqualTo("42 is the answer.");
        await Assert.That(response.Usage).IsNotNull();
        await Assert.That(response.Usage!.TotalTokens).IsEqualTo(15);
    }

    // ========================================================================
    // DeleteResult deserialization
    // ========================================================================

    [Test]
    public async Task ResponseDeleteResult_Deserializes()
    {
        var json = """{"id":"resp_abc","object":"response","deleted":true}""";
        var result = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseDeleteResult);

        await Assert.That(result).IsNotNull();
        await Assert.That(result!.Deleted).IsTrue();
        await Assert.That(result.Id).IsEqualTo("resp_abc");
    }

    // ========================================================================
    // InputItemsList deserialization
    // ========================================================================

    [Test]
    public async Task ResponseInputItemsList_Deserializes()
    {
        var json = """{"object":"list","data":[{"type":"message","role":"user","content":"Hi"}]}""";
        var list = JsonSerializer.Deserialize(json, ResponsesJsonContext.Default.ResponseInputItemsList);

        await Assert.That(list).IsNotNull();
        await Assert.That(list!.Data).HasCount().EqualTo(1);

        var msg = list.Data[0] as ResponseMessageItem;
        await Assert.That(msg).IsNotNull();
        await Assert.That(msg!.Role).IsEqualTo("user");
    }
}
