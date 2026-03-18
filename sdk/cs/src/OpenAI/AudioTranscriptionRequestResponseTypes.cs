// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

using Microsoft.Extensions.Logging;

internal record AudioTranscriptionCreateRequestExtended : AudioCreateTranscriptionRequest
{
    // Valid entries:
    // int language
    // int temperature
    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }

    internal static AudioTranscriptionCreateRequestExtended FromUserInput(string modelId,
                                                                      string audioFilePath,
                                                                      OpenAIAudioClient.AudioSettings settings)
    {
        var request = new AudioTranscriptionCreateRequestExtended
        {
            Model = modelId,
            FileName = audioFilePath,

            // apply our specific settings
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
}
internal static class AudioTranscriptionRequestResponseExtensions
{
    internal static string ToJson(this AudioCreateTranscriptionRequest request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.AudioCreateTranscriptionRequest);
    }
    internal static AudioCreateTranscriptionResponse ToAudioTranscription(this ICoreInterop.Response response,
                                                                          ILogger logger)
    {
        if (response.Error != null)
        {
            logger.LogError("Error from audio_transcribe: {Error}", response.Error);
            throw new FoundryLocalException($"Error from audio_transcribe command: {response.Error}");
        }

        return response.Data!.ToAudioTranscription(logger);
    }

    internal static AudioCreateTranscriptionResponse ToAudioTranscription(this string responseData, ILogger logger)
    {
        var typeInfo = JsonSerializationContext.Default.AudioCreateTranscriptionResponse;
        var response = JsonSerializer.Deserialize(responseData, typeInfo);
        if (response == null)
        {
            logger.LogError("Failed to deserialize AudioCreateTranscriptionResponse. Json={Data}", responseData);
            throw new FoundryLocalException("Failed to deserialize AudioCreateTranscriptionResponse");
        }

        return response;
    }
}
