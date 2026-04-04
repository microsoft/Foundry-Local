// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using Microsoft.AI.Foundry.Local.OpenAI;

internal sealed class LiveAudioTranscriptionTests
{
    // --- LiveAudioTranscriptionResponse.FromJson tests ---

    [Test]
    public async Task FromJson_ParsesTextAndIsFinal()
    {
        var json = /*lang=json,strict*/ """{"is_final":true,"text":"hello world","start_time":null,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content).IsNotNull();
        await Assert.That(result.Content!.Count).IsEqualTo(1);
        await Assert.That(result.Content[0].Text).IsEqualTo("hello world");
        await Assert.That(result.Content[0].Transcript).IsEqualTo("hello world");
        await Assert.That(result.IsFinal).IsTrue();
    }

    [Test]
    public async Task FromJson_MapsTimingFields()
    {
        var json = /*lang=json,strict*/ """{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content?[0]?.Text).IsEqualTo("partial");
        await Assert.That(result.IsFinal).IsFalse();
        await Assert.That(result.StartTime).IsEqualTo(1.5);
        await Assert.That(result.EndTime).IsEqualTo(3.0);
    }

    [Test]
    public async Task FromJson_EmptyText_ParsesSuccessfully()
    {
        var json = /*lang=json,strict*/ """{"is_final":true,"text":"","start_time":null,"end_time":null}""";

        var result = LiveAudioTranscriptionResponse.FromJson(json);

        await Assert.That(result.Content?[0]?.Text).IsEqualTo("");
        await Assert.That(result.IsFinal).IsTrue();
    }

    [Test]
    public async Task FromJson_OnlyStartTime_SetsStartTime()
    {
        var json = /*lang=json,strict*/ """{"is_final":true,"text":"word","start_time":2.0,"end_time":null}""";

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
        var json = /*lang=json,strict*/ """{"is_final":true,"text":"test","start_time":null,"end_time":null}""";

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
        var json = /*lang=json,strict*/ """{"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}""";

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
        var json = /*lang=json,strict*/ """{"code":"BUSY","message":"Model busy","isTransient":true}""";

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

    // --- E2E streaming test with synthetic PCM audio ---

    [Test]
    public async Task LiveStreaming_E2E_WithSyntheticPCM_ReturnsValidResponse()
    {
        // Skip if FoundryLocalManager is not initialized (no Core DLL / no models)
        if (!FoundryLocalManager.IsInitialized)
        {
            return;
        }

        var manager = FoundryLocalManager.Instance;
        var catalog = await manager.GetCatalogAsync();
        var model = await catalog.GetModelAsync("nemotron");

        if (model == null)
        {
            // Skip gracefully if nemotron model not available
            return;
        }

        if (!await model.IsCachedAsync())
        {
            return;
        }

        await model.LoadAsync();

        try
        {
            var audioClient = await model.GetAudioClientAsync();
            var session = audioClient.CreateLiveTranscriptionSession();
            session.Settings.SampleRate = 16000;
            session.Settings.Channels = 1;
            session.Settings.BitsPerSample = 16;

            await session.StartAsync();

            // Start collecting results in background (must start before pushing audio)
            var results = new List<LiveAudioTranscriptionResponse>();
            var readTask = Task.Run(async () =>
            {
                await foreach (var result in session.GetTranscriptionStream())
                {
                    results.Add(result);
                }
            });

            // Generate ~2 seconds of synthetic PCM audio (440Hz sine wave, 16kHz, 16-bit mono)
            const int sampleRate = 16000;
            const int durationSeconds = 2;
            const double frequency = 440.0;
            var totalSamples = sampleRate * durationSeconds;
            var pcmBytes = new byte[totalSamples * 2]; // 16-bit = 2 bytes per sample

            for (var i = 0; i < totalSamples; i++)
            {
                var t = (double)i / sampleRate;
                var sample = (short)(short.MaxValue * 0.5 * Math.Sin(2 * Math.PI * frequency * t));
                pcmBytes[i * 2] = (byte)(sample & 0xFF);
                pcmBytes[(i * 2) + 1] = (byte)((sample >> 8) & 0xFF);
            }

            // Push audio in chunks (100ms each, matching typical mic callback size)
            var chunkSize = sampleRate / 10 * 2; // 100ms of 16-bit audio
            for (var offset = 0; offset < pcmBytes.Length; offset += chunkSize)
            {
                var len = Math.Min(chunkSize, pcmBytes.Length - offset);
                await session.AppendAsync(new ReadOnlyMemory<byte>(pcmBytes, offset, len));
            }

            // Stop session to flush remaining audio and complete the stream
            await session.StopAsync();
            await readTask;

            // Verify response attributes — synthetic audio may or may not produce text,
            // but the response objects should be properly structured
            foreach (var result in results)
            {
                // Verify ConversationItem-shaped response
                await Assert.That(result.Content).IsNotNull();
                await Assert.That(result.Content!.Count).IsGreaterThan(0);
                await Assert.That(result.Content[0].Text).IsNotNull();
                // Text and Transcript should be the same
                await Assert.That(result.Content[0].Transcript).IsEqualTo(result.Content[0].Text);
            }
        }
        finally
        {
            await model.UnloadAsync();
        }
    }
}
