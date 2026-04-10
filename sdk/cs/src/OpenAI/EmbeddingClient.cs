// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

/// <summary>
/// Embedding Client that uses the OpenAI API.
/// Implemented using Betalgo.Ranul.OpenAI SDK types.
/// </summary>
public class OpenAIEmbeddingClient
{
    private readonly string _modelId;

    private readonly ICoreInterop _coreInterop = FoundryLocalManager.Instance.CoreInterop;
    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    internal OpenAIEmbeddingClient(string modelId)
    {
        _modelId = modelId;
    }

    /// <summary>
    /// Settings that are supported by Foundry Local for embeddings.
    /// </summary>
    public record EmbeddingSettings
    {
        /// <summary>
        /// The number of dimensions the resulting output embeddings should have.
        /// </summary>
        public int? Dimensions { get; set; }

        /// <summary>
        /// The format to return the embeddings in. Can be either "float" or "base64".
        /// </summary>
        public string? EncodingFormat { get; set; }
    }

    /// <summary>
    /// Settings to use for embedding requests using this client.
    /// </summary>
    public EmbeddingSettings Settings { get; } = new();

    /// <summary>
    /// Generate embeddings for the given input text.
    /// </summary>
    /// <param name="input">The text to generate embeddings for.</param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Embedding response containing the embedding vector.</returns>
    public async Task<EmbeddingCreateResponse> GenerateEmbeddingAsync(string input,
                                                                      CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => GenerateEmbeddingImplAsync(input, ct),
            "Error during embedding generation.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Generate embeddings for multiple input texts in a single request.
    /// </summary>
    /// <param name="inputs">The texts to generate embeddings for.</param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Embedding response containing one embedding vector per input.</returns>
    public async Task<EmbeddingCreateResponse> GenerateEmbeddingsAsync(IEnumerable<string> inputs,
                                                                       CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => GenerateEmbeddingsImplAsync(inputs, ct),
            "Error during batch embedding generation.", _logger).ConfigureAwait(false);
    }

    private async Task<EmbeddingCreateResponse> GenerateEmbeddingImplAsync(string input,
                                                                            CancellationToken? ct)
    {
        var embeddingRequest = EmbeddingCreateRequestExtended.FromUserInput(_modelId, input, Settings);
        var embeddingRequestJson = embeddingRequest.ToJson();

        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", embeddingRequestJson } } };
        var response = await _coreInterop.ExecuteCommandAsync("embeddings", request,
                                                                ct ?? CancellationToken.None).ConfigureAwait(false);

        return response.ToEmbeddingResponse(_logger);
    }

    private async Task<EmbeddingCreateResponse> GenerateEmbeddingsImplAsync(IEnumerable<string> inputs,
                                                                             CancellationToken? ct)
    {
        var embeddingRequest = EmbeddingCreateRequestExtended.FromUserInput(_modelId, inputs, Settings);
        var embeddingRequestJson = embeddingRequest.ToJson();

        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", embeddingRequestJson } } };
        var response = await _coreInterop.ExecuteCommandAsync("embeddings", request,
                                                                ct ?? CancellationToken.None).ConfigureAwait(false);

        return response.ToEmbeddingResponse(_logger);
    }
}
