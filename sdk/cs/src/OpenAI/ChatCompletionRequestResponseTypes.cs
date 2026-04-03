// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

// https://platform.openai.com/docs/api-reference/chat/create
internal class ChatCompletionRequest
{
    [JsonPropertyName("model")]
    public string? Model { get; set; }

    [JsonPropertyName("messages")]
    public List<ChatMessage>? Messages { get; set; }

    [JsonPropertyName("temperature")]
    public float? Temperature { get; set; }

    [JsonPropertyName("max_tokens")]
    public int? MaxTokens { get; set; }

    [JsonPropertyName("max_completion_tokens")]
    public int? MaxCompletionTokens { get; set; }

    [JsonPropertyName("n")]
    public int? N { get; set; }

    [JsonPropertyName("stream")]
    public bool? Stream { get; set; }

    [JsonPropertyName("top_p")]
    public float? TopP { get; set; }

    [JsonPropertyName("frequency_penalty")]
    public float? FrequencyPenalty { get; set; }

    [JsonPropertyName("presence_penalty")]
    public float? PresencePenalty { get; set; }

    [JsonPropertyName("stop")]
    public string? Stop { get; set; }

    [JsonPropertyName("tools")]
    public List<ToolDefinition>? Tools { get; set; }

    [JsonPropertyName("tool_choice")]
    public ToolChoice? ToolChoice { get; set; }

    [JsonPropertyName("response_format")]
    public ResponseFormatExtended? ResponseFormat { get; set; }

    // Extension: additional parameters passed via metadata
    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }

    internal static ChatCompletionRequest FromUserInput(string modelId,
                                                        IEnumerable<ChatMessage> messages,
                                                        IEnumerable<ToolDefinition>? tools,
                                                        OpenAIChatClient.ChatSettings settings)
    {
        var request = new ChatCompletionRequest
        {
            Model = modelId,
            Messages = messages.ToList(),
            Tools = tools?.ToList(),
            // Apply our specific settings
            FrequencyPenalty = settings.FrequencyPenalty,
            MaxTokens = settings.MaxTokens,
            MaxCompletionTokens = settings.MaxCompletionTokens,
            N = settings.N,
            Temperature = settings.Temperature,
            PresencePenalty = settings.PresencePenalty,
            Stop = settings.Stop,
            Stream = settings.Stream,
            TopP = settings.TopP,
            // Apply tool calling and structured output settings
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
}

internal static class ChatCompletionsRequestResponseExtensions
{
    internal static string ToJson(this ChatCompletionRequest request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.ChatCompletionRequest);
    }

    internal static ChatCompletionResponse ToChatCompletion(this ICoreInterop.Response response, ILogger logger)
    {
        if (response.Error != null)
        {
            logger.LogError("Error from chat_completions: {Error}", response.Error);
            throw new FoundryLocalException($"Error from chat_completions command: {response.Error}");
        }

        return response.Data!.ToChatCompletion(logger);
    }

    internal static ChatCompletionResponse ToChatCompletion(this string responseData, ILogger logger)
    {
        var output = JsonSerializer.Deserialize(responseData, JsonSerializationContext.Default.ChatCompletionResponse);
        if (output == null)
        {
            logger.LogError("Failed to deserialize chat completion response: {ResponseData}", responseData);
            throw new JsonException("Failed to deserialize ChatCompletion");
        }

        return output;
    }
}
