// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

using Microsoft.Extensions.Logging;

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
