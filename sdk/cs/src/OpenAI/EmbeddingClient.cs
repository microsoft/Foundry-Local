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
        if (string.IsNullOrWhiteSpace(input))
        {
            throw new ArgumentException("Input must be a non-empty string.", nameof(input));
        }

        var embeddingRequest = EmbeddingCreateRequestExtended.FromUserInput(_modelId, input);
        var embeddingRequestJson = embeddingRequest.ToJson();

        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", embeddingRequestJson } } };
        var response = await _coreInterop.ExecuteCommandAsync("embeddings", request,
                                                                ct ?? CancellationToken.None).ConfigureAwait(false);

        return response.ToEmbeddingResponse(_logger);
    }

    private async Task<EmbeddingCreateResponse> GenerateEmbeddingsImplAsync(IEnumerable<string> inputs,
                                                                             CancellationToken? ct)
    {
        if (inputs == null || !inputs.Any())
        {
            throw new ArgumentException("Inputs must be a non-empty array of strings.", nameof(inputs));
        }

        foreach (var input in inputs)
        {
            if (string.IsNullOrWhiteSpace(input))
            {
                throw new ArgumentException("Each input must be a non-empty string.", nameof(inputs));
            }
        }

        var embeddingRequest = EmbeddingCreateRequestExtended.FromUserInput(_modelId, inputs);
        var embeddingRequestJson = embeddingRequest.ToJson();

        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", embeddingRequestJson } } };
        var response = await _coreInterop.ExecuteCommandAsync("embeddings", request,
                                                                ct ?? CancellationToken.None).ConfigureAwait(false);

        return response.ToEmbeddingResponse(_logger);
    }
}
