// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

#pragma warning disable OPENAI001 // OpenAI Responses APIs are experimental in the official OpenAI package.

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.IO;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.OpenAI.Responses;

using OfficialResponses = global::OpenAI.Responses;

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

        var request = new OfficialResponses.CreateResponseOptions("m", new[] { OfficialResponses.ResponseItem.CreateUserMessageItem("hi") })
        {
            Temperature = 0.9f, // already set; settings must NOT override
        };
        settings.ApplyTo(request);

        await Assert.That(request.Temperature).IsEqualTo(0.9f);
        await Assert.That(request.MaxOutputTokenCount).IsEqualTo(64);
        await Assert.That(request.StoredOutputEnabled).IsEqualTo(false);
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
        await Assert.That(async () => await client.CreateAsync(Array.Empty<OfficialResponses.ResponseItem>()))
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
    // Image content helper factories
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task CreateInputImagePartFromBytes_Builds_DataUri()
    {
        var bytes = new byte[] { 1, 2, 3, 4 };
        var part = ResponseContentPartHelpers.CreateInputImagePartFromBytes(bytes, "image/png", "low");

        await Assert.That(part).IsNotNull();
        await Assert.That(part.Kind).IsEqualTo(OfficialResponses.ResponseContentPartKind.InputImage);
        await Assert.That(part.InputImageUri).IsNotNull().And.StartsWith("data:image/png;base64,");
        await Assert.That(part.InputImageDetailLevel?.ToString()).IsEqualTo("low");
    }

    [Test]
    public async Task CreateInputImagePartFromFile_Reads_And_Detects_Png()
    {
        var path = Path.Combine(Path.GetTempPath(), $"test-{Guid.NewGuid():N}.png");
        var bytes = new byte[] { 0x89, 0x50, 0x4E, 0x47 };
        await File.WriteAllBytesAsync(path, bytes);
        try
        {
            var part = ResponseContentPartHelpers.CreateInputImagePartFromFile(path);
            await Assert.That(part.Kind).IsEqualTo(OfficialResponses.ResponseContentPartKind.InputImage);
            await Assert.That(part.InputImageUri).IsNotNull().And.StartsWith("data:image/png;base64,");
        }
        finally
        {
            File.Delete(path);
        }
    }

    [Test]
    public async Task CreateInputImagePartFromUrl_Sets_Url()
    {
        var part = ResponseContentPartHelpers.CreateInputImagePartFromUrl("https://example.com/x.png");
        await Assert.That(part.InputImageUri).IsEqualTo("https://example.com/x.png");
    }

    [Test]
    public async Task CreateInputImagePartFromFile_Throws_When_Missing()
    {
        var missing = Path.Combine(Path.GetTempPath(), $"does-not-exist-{Guid.NewGuid():N}.png");
        await Assert.That(() => ResponseContentPartHelpers.CreateInputImagePartFromFile(missing))
            .Throws<FileNotFoundException>();
    }

    [Test]
    public async Task CreateInputImagePartFromFile_Throws_When_Extension_Unknown()
    {
        var path = Path.Combine(Path.GetTempPath(), $"test-{Guid.NewGuid():N}.unknownext");
        await File.WriteAllBytesAsync(path, new byte[] { 1, 2, 3 });
        try
        {
            await Assert.That(() => ResponseContentPartHelpers.CreateInputImagePartFromFile(path))
                .Throws<ArgumentException>();
        }
        finally
        {
            File.Delete(path);
        }
    }

    [Test]
    public async Task DetectImageMediaType_Returns_Null_For_Unknown()
    {
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("foo.unknownext")).IsNull();
    }

    [Test]
    public async Task DetectImageMediaType_Recognizes_Common_Formats()
    {
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.png")).IsEqualTo("image/png");
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.JPG")).IsEqualTo("image/jpeg");
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.jpeg")).IsEqualTo("image/jpeg");
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.gif")).IsEqualTo("image/gif");
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.webp")).IsEqualTo("image/webp");
        await Assert.That(ResponseContentPartHelpers.DetectImageMediaType("a.bmp")).IsEqualTo("image/bmp");
    }

    // -----------------------------------------------------------------------------------------------------------------
    // ListResponsesResult JSON parsing
    // -----------------------------------------------------------------------------------------------------------------

    [Test]
    public async Task ListResponsesResult_FromJson_Parses_Empty_List()
    {
        const string json = """{"object":"list","data":[],"first_id":null,"last_id":null,"has_more":false}""";
        var result = ListResponsesResult.FromJson(json);
        await Assert.That(result).IsNotNull();
        await Assert.That(result.ObjectType).IsEqualTo("list");
        await Assert.That(result.Data.Count).IsEqualTo(0);
        await Assert.That(result.HasMore).IsFalse();
    }

    [Test]
    public async Task ListResponsesResult_FromJson_Parses_Pagination_Fields()
    {
        const string json = """{"object":"list","data":[],"first_id":"r_first","last_id":"r_last","has_more":true}""";
        var result = ListResponsesResult.FromJson(json);
        await Assert.That(result.FirstId).IsEqualTo("r_first");
        await Assert.That(result.LastId).IsEqualTo("r_last");
        await Assert.That(result.HasMore).IsTrue();
    }
}

#pragma warning restore OPENAI001
