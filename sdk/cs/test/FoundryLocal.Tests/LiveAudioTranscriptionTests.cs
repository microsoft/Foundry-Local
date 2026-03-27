// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text.Json;
using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.OpenAI;

internal sealed class LiveAudioTranscriptionTests
{
    // --- LiveAudioTranscriptionResponse.FromJson tests ---

    [Test]
    public async Task FromJson_ParsesTextAndIsFinal()
    {
        var json = """{"is_final":true,"text":"hello world","start_time":null,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content).IsNotNull();
        await Assert.That(result.Content!.Count).IsEqualTo(1);
        await Assert.That(result.Content[0].Text).IsEqualTo("hello world");
        await Assert.That(result.Content[0].Transcript).IsEqualTo("hello world");
        await Assert.That(result.Content[0].Type).IsEqualTo("text");
        await Assert.That(result.IsFinal).IsTrue();
    }

    [Test]
    public async Task FromJson_MapsTimingFields()
    {
        var json = """{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content?[0]?.Text).IsEqualTo("partial");
        await Assert.That(result.IsFinal).IsFalse();
        await Assert.That(result.StartTime).IsEqualTo(1.5);
        await Assert.That(result.EndTime).IsEqualTo(3.0);
    }

    [Test]
    public async Task FromJson_EmptyText_ParsesSuccessfully()
    {
        var json = """{"is_final":true,"text":"","start_time":null,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content?[0]?.Text).IsEqualTo("");
        await Assert.That(result.IsFinal).IsTrue();
    }

    [Test]
    public async Task FromJson_OnlyStartTime_SetsStartTime()
    {
        var json = """{"is_final":true,"text":"word","start_time":2.0,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.StartTime).IsEqualTo(2.0);
        await Assert.That(result.EndTime).IsNull();
        await Assert.That(result.Content?[0]?.Text).IsEqualTo("word");
    }

    [Test]
    public async Task FromJson_InvalidJson_Throws()
    {
        var ex = Assert.Throws<Exception>(() =>
            LiveAudioTranscriptionResponse.FromJson("not valid json"));
        await Assert.That(ex).IsNotNull();
    }

    [Test]
    public async Task FromJson_ContentHasTextAndTranscript()
    {
        var json = """{"is_final":true,"text":"test","start_time":null,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        // Both Text and Transcript should have the same value
        await Assert.That(result.Content?[0]?.Text).IsEqualTo("test");
        await Assert.That(result.Content?[0]?.Transcript).IsEqualTo("test");
    }

    // --- LiveAudioTranscriptionOptions tests ---

    [Test]
    public async Task Options_DefaultValues()
    {
        var options = new LiveAudioTranscriptionSession.LiveAudioTranscriptionOptions();

        await Assert.That(options.SampleRate).IsEqualTo(16000);
        await Assert.That(options.Channels).IsEqualTo(1);
        await Assert.That(options.Language).IsNull();
        await Assert.That(options.PushQueueCapacity).IsEqualTo(100);
    }

    // --- CoreErrorResponse tests ---

    [Test]
    public async Task CoreErrorResponse_TryParse_ValidJson()
    {
        var json = """{"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}""";

        var error = CoreErrorResponse.TryParse(json);

        await Assert.That(error).IsNotNull();
        await Assert.That(error!.Code).IsEqualTo("ASR_SESSION_NOT_FOUND");
        await Assert.That(error.Message).IsEqualTo("Session not found");
        await Assert.That(error.IsTransient).IsFalse();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_InvalidJson_ReturnsNull()
    {
        var result = CoreErrorResponse.TryParse("not json");
        await Assert.That(result).IsNull();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_TransientError()
    {
        var json = """{"code":"BUSY","message":"Model busy","isTransient":true}""";

        var error = CoreErrorResponse.TryParse(json);

        await Assert.That(error).IsNotNull();
        await Assert.That(error!.IsTransient).IsTrue();
    }

    // --- Session state guard tests ---

    [Test]
    public async Task AppendAsync_BeforeStart_Throws()
    {
        await using var session = new LiveAudioTranscriptionSession("test-model");
        var data = new ReadOnlyMemory<byte>(new byte[100]);

        FoundryLocalException? caught = null;
        try
        {
            await session.AppendAsync(data);
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }

        await Assert.That(caught).IsNotNull();
    }

    [Test]
    public async Task GetTranscriptionStream_BeforeStart_Throws()
    {
        await using var session = new LiveAudioTranscriptionSession("test-model");

        FoundryLocalException? caught = null;
        try
        {
            await foreach (var _ in session.GetTranscriptionStream())
            {
                // should not reach here
            }
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }

        await Assert.That(caught).IsNotNull();
    }
}
