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

// https://platform.openai.com/docs/api-reference/chat/create
// Using the Betalgo ChatCompletionCreateRequest and extending with the `metadata` field for additional parameters
// which is part of the OpenAI spec but for some reason not part of the Betalgo request object.
internal class ChatCompletionCreateRequestExtended : ChatCompletionCreateRequest
{
    // Valid entries:
    // int top_k
    // int random_seed
    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }

    internal static ChatCompletionCreateRequestExtended FromUserInput(string modelId,
                                                                      IEnumerable<ChatMessage> messages,
                                                                      OpenAIChatClient.ChatSettings settings)
    {
        var request = new ChatCompletionCreateRequestExtended
        {
            Model = modelId,
            Messages = messages.ToList(),

            // apply our specific settings
            FrequencyPenalty = settings.FrequencyPenalty,
            MaxTokens = settings.MaxTokens,
            N = settings.N,
            Temperature = settings.Temperature,
            PresencePenalty = settings.PresencePenalty,
            Stream = settings.Stream,
            TopP = settings.TopP
        };

        var metadata = new Dictionary<string, string>();

        if (settings.TopK.HasValue)
        {
            metadata["top_k"] = settings.TopK.Value.ToString(CultureInfo.InvariantCulture);
        }

        if (settings.RandomSeed.HasValue)
        {
            metadata["random_seed"] = settings.RandomSeed.Value.ToString(CultureInfo.InvariantCulture);
        }

        if (metadata.Count > 0)
        {
            request.Metadata = metadata;
        }


        return request;
    }
}

internal static class ChatCompletionsRequestResponseExtensions
{
    internal static string ToJson(this ChatCompletionCreateRequestExtended request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.ChatCompletionCreateRequestExtended);
    }

    internal static ChatCompletionCreateResponse ToChatCompletion(this ICoreInterop.Response response, ILogger logger)
    {
        if (response.Error != null)
        {
            logger.LogError("Error from chat_completions: {Error}", response.Error);
            throw new FoundryLocalException($"Error from chat_completions command: {response.Error}");
        }

        return response.Data!.ToChatCompletion(logger);
    }

    internal static ChatCompletionCreateResponse ToChatCompletion(this string responseData, ILogger logger)
    {
        return JsonSerializer.Deserialize(responseData, JsonSerializationContext.Default.ChatCompletionCreateResponse)
            ?? throw new JsonException("Failed to deserialize ChatCompletion");
    }
}
