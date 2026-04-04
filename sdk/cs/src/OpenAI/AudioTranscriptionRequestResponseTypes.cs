// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Globalization;
using System.Text.Json;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

using Microsoft.Extensions.Logging;

internal static class AudioTranscriptionRequestResponseExtensions
{
    internal static AudioTranscriptionRequest FromUserInput(string modelId,
                                                             string audioFilePath,
                                                             OpenAIAudioClient.AudioSettings settings)
    {
        var request = new AudioTranscriptionRequest
        {
            Model = modelId,
            FileName = audioFilePath,
            Language = settings.Language,
            Temperature = settings.Temperature
        };

        var metadata = new Dictionary<string, string>();

        if (settings.Language != null)
        {
            metadata["language"] = settings.Language;
        }

        if (settings.Temperature.HasValue)
        {
            metadata["temperature"] = settings.Temperature.Value.ToString(CultureInfo.InvariantCulture);
        }

        if (metadata.Count > 0)
        {
            request.Metadata = metadata;
        }

        return request;
    }

    internal static string ToJson(this AudioTranscriptionRequest request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.AudioTranscriptionRequest);
    }

    internal static AudioTranscriptionResponse ToAudioTranscription(this ICoreInterop.Response response,
                                                                     ILogger logger)
    {
        if (response.Error != null)
        {
            logger.LogError("Error from audio_transcribe: {Error}", response.Error);
            throw new FoundryLocalException($"Error from audio_transcribe command: {response.Error}");
        }

        return response.Data!.ToAudioTranscription(logger);
    }

    internal static AudioTranscriptionResponse ToAudioTranscription(this string responseData, ILogger logger)
    {
        var typeInfo = JsonSerializationContext.Default.AudioTranscriptionResponse;
        var response = JsonSerializer.Deserialize(responseData, typeInfo);
        if (response == null)
        {
            logger.LogError("Failed to deserialize AudioTranscriptionResponse. Json={Data}", responseData);
            throw new FoundryLocalException("Failed to deserialize AudioTranscriptionResponse");
        }

        return response;
    }
}
