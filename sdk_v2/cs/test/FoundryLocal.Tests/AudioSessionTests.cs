// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

using TUnit.Core.Exceptions;

#pragma warning disable CA2000 // Items are transferred to Request via AddItem

[SkipUnlessIntegration]
internal sealed class AudioSessionTests
{
    private static IModel? model;
    private static IModel? streamingModel;

    private const string ExpectedTranscription =
        " And lots of times you need to give people more than one link at a time." +
        " You a band could give their fans a couple new videos from the live concert" +
        " behind the scenes photo gallery and album to purchase like these next few links.";

    // 100 ms at 16 kHz mono s16le = 16000 * 0.1 * 2 bytes/sample.
    private const int StreamingPcmChunkSize = 3200;

    private static readonly string[] StreamingKeyPhrases =
    [
        "give people",
        "more than one link",
        "live concert",
        "behind the scenes",
        "photo gallery",
        "album to purchase",
    ];

    [Before(Class)]
    public static async Task Setup()
    {
        try
        {
            var manager = FoundryLocalManager.Instance;
            var catalog = await manager.GetCatalogAsync();

            var aliasModel = await catalog.GetModelAsync("whisper-tiny").ConfigureAwait(false);

            if (aliasModel == null)
            {
                return;
            }

            // Pick the CPU variant — CUDA/DML variants require an EP bootstrapper that may not be registered.
            var model = aliasModel.Variants.FirstOrDefault(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);

            if (model == null)
            {
                return;
            }

            if (!await model.IsCachedAsync())
            {
                await model.DownloadAsync().ConfigureAwait(false);
                // return;
            }

            await model.LoadAsync().ConfigureAwait(false);
            await Assert.That(await model.IsLoadedAsync()).IsTrue();

            AudioSessionTests.model = model;

            // Best-effort load of the nemotron live-streaming ASR model used by the
            // streamed-in / streamed-out test below. Only this model accepts live PCM
            // chunks through an ItemQueue; whisper rejects them at inference time.
            // We don't auto-download it — the test skips when it isn't cached locally.
            var streaming = await catalog.GetModelAsync("nemotron-speech-streaming-en-0.6b").ConfigureAwait(false);

            if (streaming != null && await streaming.IsCachedAsync())
            {
                await streaming.LoadAsync().ConfigureAwait(false);
                AudioSessionTests.streamingModel = streaming;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Setup failed: {ex}");
            throw;
        }
    }

    [Test]
    public async Task Transcribe_NoStreaming_Succeeds()
    {
        if (model == null)
        {
            throw new SkipTestException("Whisper model not available");
        }

        using var session = new AudioSession(model!);
        session.SetOptions(new RequestOptions
        {
            AdditionalOptions = new Dictionary<string, string> { ["language"] = "en" },
        });

        var audioFilePath = Utils.TestDataPath("Recording.mp3");

        using var request = new Request();
        request.AddItem(new AudioItem(audioFilePath));

        using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

        await Assert.That(response).IsNotNull();

        string? text = null;
        int segmentCount = 0;

        for (int i = 0; i < response.ItemCount; i++)
        {
            using var item = response.GetItem(i);

            if (item is SpeechResultItem result)
            {
                text = result.Text;
                segmentCount = result.Segments.Count;
                break;
            }
        }

        await Assert.That(text).IsNotNull().And.IsNotEmpty();
        await Assert.That(text!).IsEqualTo(ExpectedTranscription);
        await Assert.That(segmentCount).IsGreaterThan(0);
        Console.WriteLine($"Response: {text} ({segmentCount} segments)");
    }

    [Test]
    public async Task Transcribe_Streaming_Succeeds()
    {
        if (model == null)
        {
            throw new SkipTestException("Whisper model not available");
        }

        using var session = new AudioSession(model!);
        session.SetOptions(new RequestOptions
        {
            AdditionalOptions = new Dictionary<string, string> { ["language"] = "en" },
        });
        session.SetStreaming(true);

        var audioFilePath = Utils.TestDataPath("Recording.mp3");

        using var request = new Request();
        request.AddItem(new AudioItem(audioFilePath));

        var sb = new StringBuilder();
        int callbackCount = 0;

        await foreach (var item in session.ProcessStreamingRequestAsync(request).ConfigureAwait(false))
        {
            using (item)
            {
                if (item is SpeechSegmentItem seg)
                {
                    sb.Append(seg.Text);
                    callbackCount++;
                }
            }
        }

        var fullResponse = sb.ToString();
        Console.WriteLine($"Streaming response ({callbackCount} callbacks): {fullResponse}");
        await Assert.That(callbackCount).IsGreaterThan(0);
        await Assert.That(fullResponse).IsEqualTo(ExpectedTranscription);
    }

    [Test]
    public async Task Transcribe_Streaming_FinalResponse_AggregatesTranscript()
    {
        if (model == null)
        {
            throw new SkipTestException("Whisper model not available");
        }

        using var session = new AudioSession(model!);
        session.SetOptions(new RequestOptions
        {
            AdditionalOptions = new Dictionary<string, string> { ["language"] = "en" },
        });
        session.SetStreaming(true);

        var audioFilePath = Utils.TestDataPath("Recording.mp3");

        using var request = new Request();
        request.AddItem(new AudioItem(audioFilePath));

        var stream = session.ProcessStreamingRequestAsync(request);

        var sb = new StringBuilder();
        int streamedCount = 0;

        await foreach (var item in stream)
        {
            using (item)
            {
                if (item is SpeechSegmentItem seg)
                {
                    sb.Append(seg.Text);
                    streamedCount++;
                }
            }
        }

        await Assert.That(streamedCount).IsGreaterThan(0);
        await Assert.That(sb.ToString()).IsEqualTo(ExpectedTranscription);

        using var final = await stream.FinalResponse;

        await Assert.That(final).IsNotNull();
        await Assert.That(final.ItemCount).IsGreaterThan(0);

        string? aggregated = null;
        for (int i = 0; i < final.ItemCount; i++)
        {
            var item = final.GetItem(i);
            if (item is SpeechResultItem result)
            {
                aggregated = result.Text;
                break;
            }
        }

        await Assert.That(aggregated).IsNotNull();
        await Assert.That(aggregated!).IsEqualTo(ExpectedTranscription);
    }

    [Test]
    public async Task Transcribe_InvalidFile_Throws()
    {
        if (model == null)
        {
            throw new SkipTestException("Whisper model not available");
        }

        using var session = new AudioSession(model!);

        var audioFilePath = Utils.TestDataPath("non_exist_Recording.mp3");

        using var request = new Request();
        request.AddItem(new AudioItem(audioFilePath));

        FoundryLocalException? caught = null;

        try
        {
            using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }

        await Assert.That(caught).IsNotNull();
        Console.WriteLine($"Caught exception: {caught}");
    }

    /// <summary>
    /// Streamed input + streamed output: pushes PCM chunks into a session-borrowed
    /// <see cref="ItemQueue"/> while consuming per-token <see cref="SpeechSegmentItem"/>s via
    /// <see cref="Session.ProcessStreamingRequestAsync"/>, then verifies the terminal
    /// <see cref="SpeechResultItem"/> aggregates the same segments. Mirrors the C++
    /// <c>StreamingAudioFixture.StreamingCallbackReceivesTokens</c> test in
    /// <c>sdk_v2/cpp/test/sdk_api/streaming_audio_test.cc</c>. Requires the nemotron
    /// live-streaming ASR model — whisper rejects PCM chunks at inference.
    /// </summary>
    [Test]
    public async Task Transcribe_PcmStreamedInAndSegmentsStreamedOut()
    {
        if (streamingModel == null || !await streamingModel.IsLoadedAsync())
        {
            throw new SkipTestException("nemotron streaming audio model not available");
        }

        var pcmPath = Utils.TestDataPath("Recording.pcm");

        if (!File.Exists(pcmPath))
        {
            throw new SkipTestException($"Recording.pcm not found at {pcmPath}");
        }

        var pcm = File.ReadAllBytes(pcmPath);
        await Assert.That(pcm.Length).IsGreaterThan(0);

        var chunks = SplitIntoChunks(pcm, StreamingPcmChunkSize);
        await Assert.That(chunks.Count).IsGreaterThan(1);

        // Format descriptor — no initial data, the actual bytes arrive via the queue.
        // Mirrors the C++ Item::AudioFromData("pcm", nullptr, 0, 16000, 1) shape.
        var audio = AudioItem.CreateFormatDescriptor("pcm", sampleRate: 16000, channels: 1);
        var queue = new ItemQueue();

        using var request = new Request();
        request.AddItem(audio, takeOwnership: true);
        request.AddItem(queue, takeOwnership: false);

        using var session = new AudioSession(streamingModel!);
        session.SetStreaming(true);

        // ItemQueue is unbounded, so we can push every chunk synchronously up-front.
        // The native session consumes them on its own worker thread once we kick off
        // ProcessStreamingRequestAsync below.
        foreach (var chunk in chunks)
        {
            queue.Push(new BytesItem(chunk));
        }

        queue.MarkFinished();

        var sb = new StringBuilder();
        int segmentCount = 0;

        var stream = session.ProcessStreamingRequestAsync(request);

        await foreach (var item in stream)
        {
            using (item)
            {
                if (item is SpeechSegmentItem seg)
                {
                    sb.Append(seg.Text);
                    segmentCount++;
                }
                else
                {
                    Assert.Fail($"unexpected streamed item type: {item.GetType().Name}");
                }
            }
        }

        await Assert.That(segmentCount).IsGreaterThan(0);
        var streamedText = sb.ToString();
        await Assert.That(streamedText).IsNotEmpty();
        ExpectStreamingTranscriptionContent(streamedText);

        // The terminal Response carries an aggregated SpeechResultItem built from the
        // streamed segments. Validate it matches what we accumulated from the iterator.
        using var final = await stream.FinalResponse;
        await Assert.That(final).IsNotNull();
        await Assert.That(final.ItemCount).IsGreaterThan(0);

        SpeechResultItem? aggregated = null;

        for (int i = 0; i < final.ItemCount; i++)
        {
            using var item = final.GetItem(i);

            if (item is SpeechResultItem result)
            {
                aggregated = result;
                break;
            }
        }

        await Assert.That(aggregated).IsNotNull();
        await Assert.That(aggregated!.Segments.Count).IsEqualTo(segmentCount);
        await Assert.That(aggregated.Text).IsEqualTo(streamedText);

        Console.WriteLine($"Streaming transcription ({segmentCount} segments): {streamedText}");
    }

    private static List<byte[]> SplitIntoChunks(byte[] data, int chunkSize)
    {
        var chunks = new List<byte[]>();

        for (int offset = 0; offset < data.Length; offset += chunkSize)
        {
            int len = Math.Min(chunkSize, data.Length - offset);
            var chunk = new byte[len];
            Array.Copy(data, offset, chunk, 0, len);
            chunks.Add(chunk);
        }

        return chunks;
    }

    private static void ExpectStreamingTranscriptionContent(string text)
    {
        var lower = text.ToLowerInvariant();

        foreach (var phrase in StreamingKeyPhrases)
        {
            if (!lower.Contains(phrase))
            {
                Assert.Fail($"Expected transcription to contain '{phrase}'.\nGot: {text}");
            }
        }
    }
}
