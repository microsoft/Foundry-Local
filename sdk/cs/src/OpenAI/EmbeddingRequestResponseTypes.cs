// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json;
using System.Text.Json.Serialization;

using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

// https://platform.openai.com/docs/api-reference/embeddings/create
internal record EmbeddingCreateRequestExtended
{
    [JsonPropertyName("input")]
    public string? Input { get; set; }

    [JsonPropertyName("model")]
    public string? Model { get; set; }

    [JsonPropertyName("dimensions")]
    public int? Dimensions { get; set; }

    [JsonPropertyName("encoding_format")]
    public string? EncodingFormat { get; set; }

    internal static EmbeddingCreateRequestExtended FromUserInput(string modelId,
                                                                  string input,
                                                                  OpenAIEmbeddingClient.EmbeddingSettings settings)
    {
        return new EmbeddingCreateRequestExtended
        {
            Model = modelId,
            Input = input,
            Dimensions = settings.Dimensions,
            EncodingFormat = settings.EncodingFormat
        };
    }
}

internal static class EmbeddingRequestResponseExtensions
{
    internal static string ToJson(this EmbeddingCreateRequestExtended request)
    {
        return JsonSerializer.Serialize(request, JsonSerializationContext.Default.EmbeddingCreateRequestExtended);
    }

    internal static EmbeddingCreateResponse ToEmbeddingResponse(this ICoreInterop.Response response, ILogger logger)
    {
        if (response.Error != null)
        {
            logger.LogError("Error from embeddings: {Error}", response.Error);
            throw new FoundryLocalException($"Error from embeddings command: {response.Error}");
        }

        if (string.IsNullOrWhiteSpace(response.Data))
        {
            logger.LogError("Embeddings command returned no data");
            throw new FoundryLocalException("Embeddings command returned null or empty response data");
        }

        return response.Data.ToEmbeddingResponse(logger);
    }

    internal static EmbeddingCreateResponse ToEmbeddingResponse(this string responseData, ILogger logger)
    {
        var output = JsonSerializer.Deserialize(responseData, JsonSerializationContext.Default.EmbeddingCreateResponse);
        if (output == null)
        {
            logger.LogError("Failed to deserialize EmbeddingCreateResponse (length={Length})", responseData.Length);
            throw new JsonException("Failed to deserialize EmbeddingCreateResponse");
        }

        return output;
    }
}
