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

internal static class ChatCompletionsRequestResponseExtensions
{
    internal static ChatCompletionCreateRequest CreateChatRequest(string modelId,
                                                                   IEnumerable<ChatMessage> messages,
                                                                   IEnumerable<ToolDefinition>? tools,
                                                                   OpenAIChatClient.ChatSettings settings)
    {
        var request = new ChatCompletionCreateRequest
        {
            Model = modelId,
            Messages = messages.ToList(),
            Tools = tools?.ToList(),
            FrequencyPenalty = settings.FrequencyPenalty,
            MaxTokens = settings.MaxTokens,
            N = settings.N,
            Temperature = settings.Temperature,
            PresencePenalty = settings.PresencePenalty,
            Stream = settings.Stream,
            TopP = settings.TopP,
            ResponseFormat = settings.ResponseFormat,
            ToolChoice = settings.ToolChoice
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

    internal static string ToJson(this ChatCompletionCreateRequest request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.ChatCompletionCreateRequest);
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
        var output = JsonSerializer.Deserialize(responseData, JsonSerializationContext.Default.ChatCompletionCreateResponse);
        if (output == null)
        {
            logger.LogError("Failed to deserialize chat completion response: {ResponseData}", responseData);
            throw new JsonException("Failed to deserialize ChatCompletion");
        }

        return output;
    }
}
