// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.OpenAI.Responses;

using RichardSzalay.MockHttp;

internal sealed class ResponsesClientTests
{
    private const string BaseUrl = "http://localhost:5273";

    // -----------------------------------------------------------------------------------------------------------------
    // Settings / defaults
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task Settings_Store_Defaults_To_Null()
    {
        var settings = new ResponsesClientSettings();
        await Assert.That(settings.Store).IsNull();
    }

    [Test]
    public async Task Settings_Apply_Fills_Only_Unset_Fields()
    {
        var settings = new ResponsesClientSettings
        {
            Temperature = 0.3f,
            MaxOutputTokens = 64,
            Store = false,
        };

        var request = new ResponseCreateRequest
        {
            Model = "m",
            Temperature = 0.9f, // already set; settings must NOT override
        };
        settings.ApplyTo(request);

        await Assert.That(request.Temperature).IsEqualTo(0.9f);
        await Assert.That(request.MaxOutputTokens).IsEqualTo(64);
        await Assert.That(request.Store).IsEqualTo(false);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Input validation
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task CreateAsync_Empty_String_Throws()
    {
        using var client = new OpenAIResponsesClient(BaseUrl, "m");
        await Assert.That(async () => await client.CreateAsync(string.Empty))
                    .Throws<ArgumentException>();
    }

    [Test]
    public async Task CreateAsync_Null_String_Throws()
    {
        using var client = new OpenAIResponsesClient(BaseUrl, "m");
        await Assert.That(async () => await client.CreateAsync((string)null!))
                    .Throws<ArgumentNullException>();
    }

    [Test]
    public async Task CreateAsync_Empty_List_Throws()
    {
        using var client = new OpenAIResponsesClient(BaseUrl, "m");
        await Assert.That(async () => await client.CreateAsync(new List<ResponseItem>()))
                    .Throws<ArgumentException>();
    }

    [Test]
    public async Task GetAsync_Empty_Id_Throws()
    {
        using var client = new OpenAIResponsesClient(BaseUrl, "m");
        await Assert.That(async () => await client.GetAsync(""))
                    .Throws<ArgumentException>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // OutputText convenience
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task OutputText_Concatenates_Assistant_Messages()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new MessageItem
                {
                    Role = MessageRole.Assistant,
                    Content = new MessageContent
                    {
                        Parts =
                        [
                            new OutputTextContent { Text = "hello " },
                            new OutputTextContent { Text = "world" },
                        ],
                    },
                },
            ],
        };

        await Assert.That(response.OutputText).IsEqualTo("hello world");
    }

    [Test]
    public async Task OutputText_Empty_For_No_Assistant_Message()
    {
        var response = new ResponseObject
        {
            Output =
            [
                new MessageItem
                {
                    Role = MessageRole.User,
                    Content = MessageContent.FromText("hi"),
                },
            ],
        };

        await Assert.That(response.OutputText).IsEqualTo(string.Empty);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // InputImageContent factories
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task InputImageContent_FromBytes_Sets_Data_And_Type()
    {
        var bytes = new byte[] { 1, 2, 3, 4 };
        var img = InputImageContent.FromBytes(bytes, "image/png", "low");

        await Assert.That(img.MediaType).IsEqualTo("image/png");
        await Assert.That(img.Detail).IsEqualTo("low");
        await Assert.That(img.ImageData).IsEqualTo(Convert.ToBase64String(bytes));
        await Assert.That(img.Kind).IsEqualTo("input_image");
    }

    [Test]
    public async Task InputImageContent_FromFile_Reads_And_Detects_Png()
    {
        var path = Path.Combine(Path.GetTempPath(), $"test-{Guid.NewGuid():N}.png");
        var bytes = new byte[] { 0x89, 0x50, 0x4E, 0x47 };
        await File.WriteAllBytesAsync(path, bytes);
        try
        {
            var img = InputImageContent.FromFile(path);
            await Assert.That(img.MediaType).IsEqualTo("image/png");
            await Assert.That(img.ImageData).IsEqualTo(Convert.ToBase64String(bytes));
        }
        finally
        {
            File.Delete(path);
        }
    }

    [Test]
    public async Task InputImageContent_FromUrl_Sets_Url()
    {
        var img = InputImageContent.FromUrl("https://example.com/x.png");
        await Assert.That(img.ImageUrl).IsEqualTo("https://example.com/x.png");
        await Assert.That(img.ImageData).IsNull();
    }

    [Test]
    public async Task InputImageContent_FromFile_Throws_When_Missing()
    {
        var missing = Path.Combine(Path.GetTempPath(), $"does-not-exist-{Guid.NewGuid():N}.png");
        await Assert.That(() => InputImageContent.FromFile(missing)).Throws<FileNotFoundException>();
    }

    [Test]
    public async Task InputImageContent_Validate_Throws_When_Both_Set()
    {
        var img = new InputImageContent { ImageUrl = "https://x/y.png", ImageData = "AAAA" };
        await Assert.That(() => img.Validate()).Throws<InvalidOperationException>();
    }

    [Test]
    public async Task InputImageContent_Validate_Throws_When_Neither_Set()
    {
        var img = new InputImageContent();
        await Assert.That(() => img.Validate()).Throws<InvalidOperationException>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Serialization: snake_case wire format
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task ResponseCreateRequest_Serializes_SnakeCase()
    {
        var req = new ResponseCreateRequest
        {
            Model = "phi",
            Input = "hi",
            MaxOutputTokens = 10,
            TopP = 0.5f,
            ParallelToolCalls = true,
            Store = false,
        };
        var json = JsonSerializer.Serialize(req, ResponsesSerializationContext.Default.ResponseCreateRequest);
        await Assert.That(json).Contains("\"max_output_tokens\":10");
        await Assert.That(json).Contains("\"top_p\":0.5");
        await Assert.That(json).Contains("\"parallel_tool_calls\":true");
        await Assert.That(json).Contains("\"store\":false");
        await Assert.That(json).Contains("\"input\":\"hi\"");
    }

    [Test]
    public async Task ResponseObject_Deserializes_Polymorphic_Output()
    {
        var json = """
        {
          "id": "resp_1",
          "object": "response",
          "created_at": 0,
          "status": "completed",
          "model": "m",
          "output": [
            {
              "type": "message",
              "role": "assistant",
              "content": [ { "type": "output_text", "text": "42" } ]
            }
          ],
          "store": true,
          "parallel_tool_calls": false
        }
        """;

        var obj = JsonSerializer.Deserialize(json, ResponsesSerializationContext.Default.ResponseObject);
        await Assert.That(obj).IsNotNull();
        await Assert.That(obj!.OutputText).IsEqualTo("42");
        await Assert.That(obj.Output[0]).IsTypeOf<MessageItem>();
    }

    [Test]
    public async Task MessageContent_Serializes_String_Form()
    {
        var req = new ResponseCreateRequest
        {
            Model = "m",
            Input = "plain",
        };
        var json = JsonSerializer.Serialize(req, ResponsesSerializationContext.Default.ResponseCreateRequest);
        await Assert.That(json).Contains("\"input\":\"plain\"");
    }

    [Test]
    public async Task StreamingEvent_Deserializes_Known_Types()
    {
        var json = """{"type":"response.output_text.delta","sequence_number":3,"item_id":"i1","output_index":0,"content_index":0,"delta":"hi"}""";
        var ev = JsonSerializer.Deserialize(json, ResponsesSerializationContext.Default.StreamingEvent);
        await Assert.That(ev).IsTypeOf<OutputTextDeltaEvent>();
        var delta = (OutputTextDeltaEvent)ev!;
        await Assert.That(delta.Delta).IsEqualTo("hi");
        await Assert.That(delta.SequenceNumber).IsEqualTo(3);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // SSE parsing (via CreateStreamingAsync)
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task Streaming_Parses_Events_And_Stops_On_Done()
    {
        var sse = new StringBuilder();
        sse.Append("event: response.created\n");
        sse.Append("data: {\"type\":\"response.created\",\"sequence_number\":0,\"response\":{\"id\":\"r1\",\"object\":\"response\",\"created_at\":0,\"status\":\"in_progress\",\"model\":\"m\",\"output\":[],\"tools\":[],\"parallel_tool_calls\":false,\"store\":true}}\n\n");
        sse.Append("event: response.output_text.delta\n");
        sse.Append("data: {\"type\":\"response.output_text.delta\",\"sequence_number\":1,\"item_id\":\"i1\",\"output_index\":0,\"content_index\":0,\"delta\":\"hi\"}\n\n");
        sse.Append("data: [DONE]\n\n");

        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Post, BaseUrl + "/v1/responses")
            .Respond(new HttpResponseMessage(HttpStatusCode.OK)
            {
                Content = new StringContent(sse.ToString(), Encoding.UTF8, "text/event-stream"),
            });

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "m", ownsClient: false);

        var events = new List<StreamingEvent>();
        await foreach (var ev in client.CreateStreamingAsync("hello"))
        {
            events.Add(ev);
        }

        await Assert.That(events.Count).IsEqualTo(2);
        await Assert.That(events[0]).IsTypeOf<ResponseCreatedEvent>();
        await Assert.That(events[1]).IsTypeOf<OutputTextDeltaEvent>();
    }

    [Test]
    public async Task Streaming_Handles_Multiline_Data()
    {
        // A payload split across two data: lines should be re-joined with \n.
        var sse = "data: {\"type\":\"response.output_text.delta\",\n"
                + "data: \"sequence_number\":1,\"item_id\":\"i1\",\"output_index\":0,\"content_index\":0,\"delta\":\"x\"}\n\n"
                + "data: [DONE]\n\n";

        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Post, BaseUrl + "/v1/responses")
            .Respond(new HttpResponseMessage(HttpStatusCode.OK)
            {
                Content = new StringContent(sse, Encoding.UTF8, "text/event-stream"),
            });

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "m", ownsClient: false);

        var events = new List<StreamingEvent>();
        await foreach (var ev in client.CreateStreamingAsync("hi"))
        {
            events.Add(ev);
        }

        await Assert.That(events.Count).IsEqualTo(1);
        await Assert.That(events[0]).IsTypeOf<OutputTextDeltaEvent>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Non-streaming round-trip via mocked HTTP
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task CreateAsync_Serializes_Request_And_Parses_Response()
    {
        string? capturedBody = null;
        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Post, BaseUrl + "/v1/responses")
            .With(req =>
            {
                capturedBody = req.Content?.ReadAsStringAsync().GetAwaiter().GetResult();
                return true;
            })
            .Respond("application/json", """
                {"id":"r1","object":"response","created_at":1,"status":"completed","model":"phi","output":[{"type":"message","role":"assistant","content":[{"type":"output_text","text":"42"}]}],"tools":[],"parallel_tool_calls":false,"store":true}
                """);

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "phi", ownsClient: false);
        client.Settings.Temperature = 0.1f;

        var result = await client.CreateAsync("What is 7*6?", req => req.MaxOutputTokens = 20);

        await Assert.That(result.Id).IsEqualTo("r1");
        await Assert.That(result.OutputText).IsEqualTo("42");
        await Assert.That(capturedBody).IsNotNull();
        await Assert.That(capturedBody!).Contains("\"stream\":false");
        await Assert.That(capturedBody!).Contains("\"max_output_tokens\":20");
        await Assert.That(capturedBody!).Contains("\"temperature\":0.1");
        await Assert.That(capturedBody!).Contains("\"model\":\"phi\"");
    }

    [Test]
    public async Task Error_Response_Throws_FoundryLocalException_With_Message()
    {
        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Post, BaseUrl + "/v1/responses")
            .Respond(HttpStatusCode.BadRequest, "application/json",
                """{"error":{"message":"bad model","type":"invalid_request_error"}}""");

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "phi", ownsClient: false);

        var ex = await Assert.That(async () => await client.CreateAsync("hi"))
                             .Throws<FoundryLocalException>();
        await Assert.That(ex!.Message).Contains("bad model");
    }

    // -----------------------------------------------------------------------------------------------------------------
    // CRUD methods
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task DeleteAsync_Returns_Result()
    {
        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Delete, BaseUrl + "/v1/responses/r1")
            .Respond("application/json", """{"id":"r1","object":"response","deleted":true}""");

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "m", ownsClient: false);
        var result = await client.DeleteAsync("r1");
        await Assert.That(result.Deleted).IsTrue();
        await Assert.That(result.Id).IsEqualTo("r1");
    }

    [Test]
    public async Task GetInputItemsAsync_Returns_List()
    {
        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Get, BaseUrl + "/v1/responses/r1/input_items")
            .Respond("application/json",
                """{"object":"list","data":[{"type":"message","role":"user","content":"hi"}],"has_more":false}""");

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "m", ownsClient: false);
        var list = await client.GetInputItemsAsync("r1");
        await Assert.That(list.Data.Count).IsEqualTo(1);
        await Assert.That(list.Data[0]).IsTypeOf<MessageItem>();
    }

    [Test]
    public async Task CancelAsync_Posts_To_Cancel_Endpoint()
    {
        var mock = new MockHttpMessageHandler();
        mock.When(HttpMethod.Post, BaseUrl + "/v1/responses/r1/cancel")
            .Respond("application/json",
                """{"id":"r1","object":"response","created_at":0,"status":"cancelled","model":"m","output":[],"tools":[],"parallel_tool_calls":false,"store":true}""");

        using var http = mock.ToHttpClient();
        using var client = new OpenAIResponsesClient(http, BaseUrl, "m", ownsClient: false);
        var result = await client.CancelAsync("r1");
        await Assert.That(result.Id).IsEqualTo("r1");
        await Assert.That(result.Status).IsEqualTo(ResponseStatus.Cancelled);
    }
}
