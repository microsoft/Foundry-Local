// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Threading.Channels;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

using NativeModel = Microsoft.AI.Foundry.Local.Detail.Native.Model;
using NativeSession = Microsoft.AI.Foundry.Local.Detail.Native.Session;

/// <summary>
/// Audio Client that uses the OpenAI API.
/// Implemented using Betalgo.Ranul.OpenAI SDK types.
/// </summary>
public class OpenAIAudioClient
{
    private readonly string _modelId;
    private readonly NativeModel _nativeModel;

    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    internal OpenAIAudioClient(string modelId, NativeModel nativeModel)
    {
        _modelId = modelId;
        _nativeModel = nativeModel;
    }

    /// <summary>
    /// Settings that are supported by Foundry Local
    /// </summary>
    public record AudioSettings
    {
        public string? Language { get; set; }
        public float? Temperature { get; set; }
    }

    /// <summary>
    /// Settings to use for audio transcription using this client.
    /// </summary>
    public AudioSettings Settings { get; } = new();

    /// <summary>
    /// Transcribe audio from a file.
    /// </summary>
    /// <param name="audioFilePath">
    /// Path to file containing audio recording.
    /// Supported formats: mp3
    /// </param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Transcription response.</returns>
    public async Task<AudioCreateTranscriptionResponse> TranscribeAudioAsync(string audioFilePath,
                                                                             CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => TranscribeAudioImplAsync(audioFilePath, ct),
                                                     "Error during audio transcription.", _logger)
                                                    .ConfigureAwait(false);
    }

    /// <summary>
    /// Transcribe audio from a file with streamed output.
    /// </summary>
    /// <param name="audioFilePath">
    /// Path to file containing audio recording.
    /// Supported formats: mp3
    /// </param>
    /// <param name="ct">Cancellation token.</param>
    /// <returns>An asynchronous enumerable of transcription responses.</returns>
    public async IAsyncEnumerable<AudioCreateTranscriptionResponse> TranscribeAudioStreamingAsync(
        string audioFilePath, [EnumeratorCancellation] CancellationToken ct)
    {
        var enumerable = Utils.CallWithExceptionHandling(
            () => TranscribeAudioStreamingImplAsync(audioFilePath, ct),
            "Error during streaming audio transcription.", _logger).ConfigureAwait(false);

        await foreach (var item in enumerable)
        {
            yield return item;
        }
    }

    /// <summary>
    /// Create a real-time streaming transcription session.
    /// Audio data is pushed in as PCM chunks and transcription results are returned as an async stream.
    /// </summary>
    /// <returns>A streaming session that must be disposed when done.</returns>
    public LiveAudioTranscriptionSession CreateLiveTranscriptionSession()
    {
        return new LiveAudioTranscriptionSession(_modelId, _nativeModel);
    }

    private async Task<AudioCreateTranscriptionResponse> TranscribeAudioImplAsync(string audioFilePath,
                                                                                  CancellationToken? ct)
    {
        var openaiRequest = AudioTranscriptionCreateRequestExtended.FromUserInput(_modelId, audioFilePath, Settings);
        var requestJson = openaiRequest.ToJson();

        return await Task.Run(() =>
        {
            using var session = new NativeSession(_nativeModel);
            using var jsonItem = new TextItem(requestJson, TextItemType.OpenAIJson);
            using var request = new Request();
            request.AddItem(jsonItem);

            var responsePtr = session.ProcessRequest(request.Ptr);
            using var response = new Response(responsePtr);

            using var responseItem = response.GetItem(0);
            var responseJson = ((TextItem)responseItem).Text;

            return JsonSerializer.Deserialize(responseJson,
                       JsonSerializationContext.Default.AudioCreateTranscriptionResponse)
                   ?? throw new FoundryLocalException("Failed to deserialize audio transcription response.");
        }).ConfigureAwait(false);
    }

    private async IAsyncEnumerable<AudioCreateTranscriptionResponse> TranscribeAudioStreamingImplAsync(
        string audioFilePath, [EnumeratorCancellation] CancellationToken ct)
    {
        var openaiRequest = AudioTranscriptionCreateRequestExtended.FromUserInput(_modelId, audioFilePath, Settings);
        var requestJson = openaiRequest.ToJson();

        var channel = Channel.CreateUnbounded<AudioCreateTranscriptionResponse>(
                        new UnboundedChannelOptions
                        {
                            SingleWriter = true,
                            SingleReader = true,
                            AllowSynchronousContinuations = true
                        });

        _ = Task.Run(() =>
        {
            try
            {
                using var session = new NativeSession(_nativeModel);

                FlStreamingCallback streamingCallback = (FlStreamingCallbackData data, IntPtr userData) =>
                {
                    try
                    {
                        if (data.ItemQueue != IntPtr.Zero)
                        {
                            while (Api.Item.QueueTryPop(data.ItemQueue, out var itemPtr))
                            {
                                using var item = Item.FromNative(itemPtr, ownsHandle: true);
                                var responseJson = ((TextItem)item).Text;

                                var chunk = JsonSerializer.Deserialize(responseJson,
                                    JsonSerializationContext.Default.AudioCreateTranscriptionResponse);

                                if (chunk != null)
                                {
                                    channel.Writer.TryWrite(chunk);
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        channel.Writer.TryComplete(
                            new FoundryLocalException("Error processing streaming audio transcription callback data.", ex, _logger));
                    }

                    return ct.IsCancellationRequested ? 1 : 0;
                };

                session.SetStreamingCallback(streamingCallback);

                using var jsonItem = new TextItem(requestJson, TextItemType.OpenAIJson);
                using var request = new Request();
                request.AddItem(jsonItem);

                var responsePtr = session.ProcessRequest(request.Ptr);
                Api.Inference.ResponseRelease(responsePtr);

                channel.Writer.TryComplete();
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error executing streaming audio transcription.", ex, _logger));
            }
            catch (OperationCanceledException)
            {
                channel.Writer.TryComplete();
            }
        }, ct);

        await foreach (var item in channel.Reader.ReadAllAsync(ct))
        {
            yield return item;
        }
    }
}
