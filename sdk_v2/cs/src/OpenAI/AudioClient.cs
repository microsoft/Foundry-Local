// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

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
    /// Create a real-time streaming transcription session.
    /// Audio data is pushed in as PCM chunks and transcription results are returned as an async stream.
    /// </summary>
    /// <returns>A streaming session that must be disposed when done.</returns>
    public AudioTranscriptionStreamSession CreateStreamingSession()
    {
        return new AudioTranscriptionStreamSession(_modelId);
    }

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

    private async Task<AudioCreateTranscriptionResponse> TranscribeAudioImplAsync(string audioFilePath,
                                                                                  CancellationToken? ct)
    {
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
}
