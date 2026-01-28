// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Runtime.CompilerServices;
using System.Threading.Channels;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

/// <summary>
/// Audio Client that uses the OpenAI API.
/// Implemented using Betalgo.Ranul.OpenAI SDK types.
/// </summary>
public class OpenAIAudioClient
{
    private readonly string _modelId;

    private readonly ICoreInterop _coreInterop = FoundryLocalManager.Instance.CoreInterop;
    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    internal OpenAIAudioClient(string modelId)
    {
        _modelId = modelId;
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
    /// Settings to use for chat completions using this client.
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

    private async Task<AudioCreateTranscriptionResponse> TranscribeAudioImplAsync(string audioFilePath,
                                                                                  CancellationToken? ct)
    {
        /*var openaiRequest = new AudioCreateTranscriptionRequest
        {
            Model = _modelId,
            FileName = audioFilePath,
            Language = language,
            Temperature = temperature
        };
        */

        var openaiRequest = AudioTranscriptionCreateRequestExtended.FromUserInput(_modelId, audioFilePath, Settings);


        var request = new CoreInteropRequest
        {
            Params = new Dictionary<string, string>
            {
                { "OpenAICreateRequest",  openaiRequest.ToJson() },
            }
        };

        var response = await _coreInterop.ExecuteCommandAsync("audio_transcribe", request,
                                                              ct ?? CancellationToken.None).ConfigureAwait(false);


        var output = response.ToAudioTranscription(_logger);

        return output;
    }

    private async IAsyncEnumerable<AudioCreateTranscriptionResponse> TranscribeAudioStreamingImplAsync(
        string audioFilePath, [EnumeratorCancellation] CancellationToken ct)
    {
        /*var openaiRequest = new AudioCreateTranscriptionRequest
        {
            Model = _modelId,
            FileName = audioFilePath
        };*/
        var openaiRequest = AudioTranscriptionCreateRequestExtended.FromUserInput(_modelId, audioFilePath, Settings);

        var request = new CoreInteropRequest
        {
            Params = new Dictionary<string, string>
            {
                { "OpenAICreateRequest",  openaiRequest.ToJson() },
            }
        };

        var channel = Channel.CreateUnbounded<AudioCreateTranscriptionResponse>(
                        new UnboundedChannelOptions
                        {
                            SingleWriter = true,
                            SingleReader = true,
                            AllowSynchronousContinuations = true
                        });

        // The callback will push ChatResponse objects into the channel.
        // The channel reader will return the values to the user.
        // This setup prevents the user from blocking the thread generating the responses.
        _ = Task.Run(async () =>
        {
            try
            {
                var failed = false;

                await _coreInterop.ExecuteCommandWithCallbackAsync(
                    "audio_transcribe",
                    request,
                    async (callbackData) =>
                    {
                        try
                        {
                            if (!failed)
                            {
                                var audioCompletion = callbackData.ToAudioTranscription(_logger);
                                await channel.Writer.WriteAsync(audioCompletion);
                            }
                        }
                        catch (Exception ex)
                        {
                            // propagate exception to reader
                            channel.Writer.TryComplete(
                                new FoundryLocalException(
                                    "Error processing streaming audio transcription callback data.", ex, _logger));
                            failed = true;
                        }
                    },
                    ct
                ).ConfigureAwait(false);

                // use TryComplete as an exception in the callback may have already closed the channel
                _ = channel.Writer.TryComplete();
            }
            // Ignore cancellation exceptions so we don't convert them into errors
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error executing streaming chat completion.", ex, _logger));
            }
            catch (OperationCanceledException)
            {
                // Complete the channel on cancellation but don't turn it into an error
                channel.Writer.TryComplete();
            }
        }, ct);

        // Start reading from the channel as items arrive.
        // This will continue until ExecuteCommandWithCallbackAsync completes and closes the channel.
        await foreach (var item in channel.Reader.ReadAllAsync(ct))
        {
            yield return item;
        }
    }
}
