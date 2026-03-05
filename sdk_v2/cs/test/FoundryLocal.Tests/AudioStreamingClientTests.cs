// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;
using Microsoft.AI.Foundry.Local.Detail;

/// <summary>
/// Unit tests for audio streaming types and settings.
/// These test the serialization, deserialization, and settings behavior
/// without requiring the native library or a loaded model.
/// </summary>
internal sealed class AudioStreamingClientTests
{
    // --- AudioStreamTranscriptionResult deserialization tests ---

    [Test]
    public async Task AudioStreamTranscriptionResult_FromJson_FinalResult_AllFields()
    {
        var json = """{"text":"hello world","is_final":true,"start_time":0.0,"end_time":1.5,"confidence":0.95}""";

        var result = AudioStreamTranscriptionResult.FromJson(json);

        await Assert.That(result).IsNotNull();
        await Assert.That(result.Text).IsEqualTo("hello world");
        await Assert.That(result.IsFinal).IsTrue();
        await Assert.That(result.StartTime).IsEqualTo(0.0);
        await Assert.That(result.EndTime).IsEqualTo(1.5);
        await Assert.That(result.Confidence).IsEqualTo(0.95f);
    }

    [Test]
    public async Task AudioStreamTranscriptionResult_FromJson_PartialResult_OptionalFieldsNull()
    {
        var json = """{"text":"hel","is_final":false}""";

        var result = AudioStreamTranscriptionResult.FromJson(json);

        await Assert.That(result).IsNotNull();
        await Assert.That(result.Text).IsEqualTo("hel");
        await Assert.That(result.IsFinal).IsFalse();
        await Assert.That(result.StartTime).IsNull();
        await Assert.That(result.EndTime).IsNull();
        await Assert.That(result.Confidence).IsNull();
    }

    [Test]
    public async Task AudioStreamTranscriptionResult_FromJson_EmptyText()
    {
        var json = """{"text":"","is_final":false}""";

        var result = AudioStreamTranscriptionResult.FromJson(json);

        await Assert.That(result).IsNotNull();
        await Assert.That(result.Text).IsEqualTo(string.Empty);
        await Assert.That(result.IsFinal).IsFalse();
    }

    [Test]
    public async Task AudioStreamTranscriptionResult_FromJson_InvalidJson_Throws()
    {
        FoundryLocalException? caught = null;
        try
        {
            AudioStreamTranscriptionResult.FromJson("not valid json");
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }
        catch (System.Text.Json.JsonException)
        {
            // Also acceptable — JsonSerializer may throw before our wrapper
            caught = new FoundryLocalException("json parse error");
        }

        await Assert.That(caught).IsNotNull();
    }

    [Test]
    public async Task AudioStreamTranscriptionResult_FromJson_EmptyJson_Throws()
    {
        FoundryLocalException? caught = null;
        try
        {
            AudioStreamTranscriptionResult.FromJson("");
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }
        catch (System.Text.Json.JsonException)
        {
            caught = new FoundryLocalException("json parse error");
        }

        await Assert.That(caught).IsNotNull();
    }

    // --- CoreErrorResponse parsing tests ---

    [Test]
    public async Task CoreErrorResponse_TryParse_TransientError_Succeeds()
    {
        var json = """{"code":"ASR_BACKEND_OVERLOADED","message":"try again later","isTransient":true}""";

        var error = CoreErrorResponse.TryParse(json);

        await Assert.That(error).IsNotNull();
        await Assert.That(error!.Code).IsEqualTo("ASR_BACKEND_OVERLOADED");
        await Assert.That(error.Message).IsEqualTo("try again later");
        await Assert.That(error.IsTransient).IsTrue();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_PermanentError_Succeeds()
    {
        var json = """{"code":"ASR_SESSION_NOT_FOUND","message":"session gone","isTransient":false}""";

        var error = CoreErrorResponse.TryParse(json);

        await Assert.That(error).IsNotNull();
        await Assert.That(error!.Code).IsEqualTo("ASR_SESSION_NOT_FOUND");
        await Assert.That(error.IsTransient).IsFalse();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_InvalidJson_ReturnsNull()
    {
        var error = CoreErrorResponse.TryParse("not json at all");

        await Assert.That(error).IsNull();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_EmptyString_ReturnsNull()
    {
        var error = CoreErrorResponse.TryParse("");

        await Assert.That(error).IsNull();
    }

    [Test]
    public async Task CoreErrorResponse_TryParse_ValidJsonWrongShape_ReturnsDefaultValues()
    {
        // Valid JSON but no matching fields — should deserialize with defaults
        var json = """{"unrelated":"field"}""";

        var error = CoreErrorResponse.TryParse(json);

        await Assert.That(error).IsNotNull();
        await Assert.That(error!.Code).IsEqualTo("");
        await Assert.That(error.IsTransient).IsFalse();
    }

    // --- StreamingAudioSettings tests ---

    [Test]
    public async Task StreamingAudioSettings_Defaults_AreCorrect()
    {
        var settings = new OpenAIAudioStreamingClient.StreamingAudioSettings();

        await Assert.That(settings.SampleRate).IsEqualTo(16000);
        await Assert.That(settings.Channels).IsEqualTo(1);
        await Assert.That(settings.BitsPerSample).IsEqualTo(16);
        await Assert.That(settings.Language).IsNull();
        await Assert.That(settings.PushQueueCapacity).IsEqualTo(100);
    }

    [Test]
    public async Task StreamingAudioSettings_Snapshot_IsIndependentCopy()
    {
        var settings = new OpenAIAudioStreamingClient.StreamingAudioSettings
        {
            SampleRate = 44100,
            Channels = 2,
            BitsPerSample = 32,
            Language = "zh",
            PushQueueCapacity = 50
        };

        var snapshot = settings.Snapshot();

        // Modify original after snapshot
        settings.SampleRate = 8000;
        settings.Channels = 1;
        settings.Language = "fr";
        settings.PushQueueCapacity = 200;

        // Snapshot should retain original values
        await Assert.That(snapshot.SampleRate).IsEqualTo(44100);
        await Assert.That(snapshot.Channels).IsEqualTo(2);
        await Assert.That(snapshot.BitsPerSample).IsEqualTo(32);
        await Assert.That(snapshot.Language).IsEqualTo("zh");
        await Assert.That(snapshot.PushQueueCapacity).IsEqualTo(50);
    }

    [Test]
    public async Task StreamingAudioSettings_Snapshot_DoesNotAffectOriginal()
    {
        var settings = new OpenAIAudioStreamingClient.StreamingAudioSettings
        {
            SampleRate = 16000,
            Language = "en"
        };

        var snapshot = settings.Snapshot();

        // Modify snapshot
        snapshot.SampleRate = 48000;
        snapshot.Language = "de";

        // Original should be unaffected
        await Assert.That(settings.SampleRate).IsEqualTo(16000);
        await Assert.That(settings.Language).IsEqualTo("en");
    }
}
