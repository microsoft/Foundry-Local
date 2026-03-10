// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Net.Http;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;

using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

/// <summary>
/// Client for the OpenAI Responses API served by Foundry Local's embedded web service.
///
/// Unlike <see cref="OpenAIChatClient"/> and <see cref="OpenAIAudioClient"/> (which use FFI via CoreInterop),
/// the Responses API is HTTP-only. This client uses HttpClient for all operations and parses
/// Server-Sent Events for streaming.
///
/// Create via <see cref="FoundryLocalManager.GetResponsesClient"/> or
/// <see cref="IModel.GetResponsesClientAsync"/>.
/// </summary>
public class OpenAIResponsesClient : IDisposable
{
    private readonly string _baseUrl;
    private readonly string? _modelId;
    private readonly HttpClient _httpClient;
    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;
    private bool _disposed;

    /// <summary>
    /// Settings for the Responses API client.
    /// </summary>
    public ResponsesSettings Settings { get; } = new();

    internal OpenAIResponsesClient(string baseUrl, string? modelId)
    {
        if (string.IsNullOrWhiteSpace(baseUrl))
        {
            throw new ArgumentException("baseUrl must be a non-empty string.", nameof(baseUrl));
        }

        _baseUrl = baseUrl.TrimEnd('/');
        _modelId = modelId;
#pragma warning disable IDISP014 // Use a single instance of HttpClient — lifetime is tied to this client
        _httpClient = new HttpClient();
#pragma warning restore IDISP014
    }

    /// <summary>
    /// Creates a model response (non-streaming).
    /// </summary>
    /// <param name="input">A string prompt or structured input.</param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>The completed Response object. Check Status and Error even on HTTP 200.</returns>
    public async Task<ResponseObject> CreateAsync(string input, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CreateImplAsync(new ResponseInput { Text = input }, options: null, ct),
            "Error creating response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Creates a model response (non-streaming) with additional options.
    /// </summary>
    public async Task<ResponseObject> CreateAsync(string input, Action<ResponseCreateRequest>? options,
                                                   CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CreateImplAsync(new ResponseInput { Text = input }, options, ct),
            "Error creating response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Creates a model response (non-streaming) from structured input items.
    /// </summary>
    public async Task<ResponseObject> CreateAsync(List<ResponseItem> input, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CreateImplAsync(new ResponseInput { Items = input }, options: null, ct),
            "Error creating response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Creates a model response (non-streaming) from structured input items with options.
    /// </summary>
    public async Task<ResponseObject> CreateAsync(List<ResponseItem> input, Action<ResponseCreateRequest>? options,
                                                   CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CreateImplAsync(new ResponseInput { Items = input }, options, ct),
            "Error creating response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Creates a streaming response, returning events as an async enumerable.
    /// </summary>
    /// <param name="input">A string prompt.</param>
    /// <param name="ct">Cancellation token.</param>
    /// <returns>Async enumerable of streaming events.</returns>
    public IAsyncEnumerable<ResponseStreamingEvent> CreateStreamingAsync(string input,
                                                                         CancellationToken ct)
    {
        return CreateStreamingAsync(input, options: null, ct);
    }

    /// <summary>
    /// Creates a streaming response with options.
    /// </summary>
    public async IAsyncEnumerable<ResponseStreamingEvent> CreateStreamingAsync(string input,
                                                                                Action<ResponseCreateRequest>? options,
                                                                                [EnumeratorCancellation] CancellationToken ct)
    {
        var enumerable = Utils.CallWithExceptionHandling(
            () => StreamingImplAsync(new ResponseInput { Text = input }, options, ct),
            "Error during streaming response.", _logger).ConfigureAwait(false);

        await foreach (var item in enumerable)
        {
            yield return item;
        }
    }

    /// <summary>
    /// Creates a streaming response from structured input items.
    /// </summary>
    public IAsyncEnumerable<ResponseStreamingEvent> CreateStreamingAsync(List<ResponseItem> input,
                                                                         CancellationToken ct)
    {
        return CreateStreamingAsync(input, options: null, ct);
    }

    /// <summary>
    /// Creates a streaming response from structured input items with options.
    /// </summary>
    public async IAsyncEnumerable<ResponseStreamingEvent> CreateStreamingAsync(List<ResponseItem> input,
                                                                                Action<ResponseCreateRequest>? options,
                                                                                [EnumeratorCancellation] CancellationToken ct)
    {
        var enumerable = Utils.CallWithExceptionHandling(
            () => StreamingImplAsync(new ResponseInput { Items = input }, options, ct),
            "Error during streaming response.", _logger).ConfigureAwait(false);

        await foreach (var item in enumerable)
        {
            yield return item;
        }
    }

    /// <summary>
    /// Retrieves a stored response by ID.
    /// </summary>
    public async Task<ResponseObject> GetAsync(string responseId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => GetImplAsync(responseId, ct),
            "Error retrieving response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Deletes a stored response by ID.
    /// </summary>
    public async Task<ResponseDeleteResult> DeleteAsync(string responseId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => DeleteImplAsync(responseId, ct),
            "Error deleting response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Cancels an in-progress response.
    /// </summary>
    public async Task<ResponseObject> CancelAsync(string responseId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CancelImplAsync(responseId, ct),
            "Error cancelling response.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Retrieves the input items for a stored response.
    /// </summary>
    public async Task<ResponseInputItemsList> GetInputItemsAsync(string responseId,
                                                                  CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => GetInputItemsImplAsync(responseId, ct),
            "Error retrieving input items.", _logger).ConfigureAwait(false);
    }

    // ========================================================================
    // Implementation methods
    // ========================================================================

    private async Task<ResponseObject> CreateImplAsync(ResponseInput input,
                                                        Action<ResponseCreateRequest>? options,
                                                        CancellationToken? ct)
    {
        var request = BuildRequest(input, stream: false);
        options?.Invoke(request);

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);
        using var content = new StringContent(json, Encoding.UTF8, "application/json");

        using var response = await _httpClient.PostAsync($"{_baseUrl}/v1/responses", content,
                                                    ct ?? CancellationToken.None).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct ?? CancellationToken.None).ConfigureAwait(false);

        var body = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        return JsonSerializer.Deserialize(body, ResponsesJsonContext.Default.ResponseObject)
               ?? throw new FoundryLocalException($"Failed to deserialize response: {body[..Math.Min(body.Length, 200)]}", _logger);
    }

    private async IAsyncEnumerable<ResponseStreamingEvent> StreamingImplAsync(
        ResponseInput input,
        Action<ResponseCreateRequest>? options,
        [EnumeratorCancellation] CancellationToken ct)
    {
        var request = BuildRequest(input, stream: true);
        options?.Invoke(request);
        // Ensure streaming stays enabled even if options attempts to override it.
        request.Stream = true;

        var json = JsonSerializer.Serialize(request, ResponsesJsonContext.Default.ResponseCreateRequest);
        using var httpRequest = new HttpRequestMessage(HttpMethod.Post, $"{_baseUrl}/v1/responses")
        {
            Content = new StringContent(json, Encoding.UTF8, "application/json")
        };
        httpRequest.Headers.Accept.Add(new System.Net.Http.Headers.MediaTypeWithQualityHeaderValue("text/event-stream"));

        HttpResponseMessage? response = null;
        try
        {
            response = await _httpClient.SendAsync(httpRequest, HttpCompletionOption.ResponseHeadersRead, ct)
                                            .ConfigureAwait(false);
            await EnsureSuccessAsync(response, ct).ConfigureAwait(false);

            var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
            using var reader = new StreamReader(stream);

            var dataLines = new List<string>();

            while (!reader.EndOfStream && !ct.IsCancellationRequested)
            {
                var line = await reader.ReadLineAsync(ct).ConfigureAwait(false);

                if (line == null)
                {
                    break;
                }

                // Empty line = end of SSE block
                if (line.Length == 0)
                {
                    if (dataLines.Count > 0)
                    {
                        var eventData = string.Join("\n", dataLines);
                        dataLines.Clear();

                        // Terminal signal
                        if (eventData == "[DONE]")
                        {
                            yield break;
                        }

                        ResponseStreamingEvent? evt;
                        try
                        {
                            evt = JsonSerializer.Deserialize(eventData, ResponsesJsonContext.Default.ResponseStreamingEvent);
                        }
                        catch (JsonException ex)
                        {
                            _logger.LogWarning(ex, "Failed to parse SSE event: {Data}", eventData);
                            continue;
                        }

                        if (evt != null)
                        {
                            yield return evt;
                        }
                    }

                    continue;
                }

                // Collect data lines
                if (line.StartsWith("data: ", StringComparison.Ordinal))
                {
                    dataLines.Add(line[6..]);
                }
                else if (line == "data:")
                {
                    dataLines.Add(string.Empty);
                }
                // 'event:' lines are informational; type is inside the JSON
            }
        }
        finally
        {
            response?.Dispose();
        }
    }

    private async Task<ResponseObject> GetImplAsync(string responseId, CancellationToken? ct)
    {
        ValidateId(responseId, nameof(responseId));
        using var response = await _httpClient.GetAsync(
            $"{_baseUrl}/v1/responses/{Uri.EscapeDataString(responseId)}",
            ct ?? CancellationToken.None).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct ?? CancellationToken.None).ConfigureAwait(false);

        var body = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        return JsonSerializer.Deserialize(body, ResponsesJsonContext.Default.ResponseObject)
               ?? throw new FoundryLocalException($"Failed to deserialize response: {body[..Math.Min(body.Length, 200)]}", _logger);
    }

    private async Task<ResponseDeleteResult> DeleteImplAsync(string responseId, CancellationToken? ct)
    {
        ValidateId(responseId, nameof(responseId));
        using var response = await _httpClient.DeleteAsync(
            $"{_baseUrl}/v1/responses/{Uri.EscapeDataString(responseId)}",
            ct ?? CancellationToken.None).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct ?? CancellationToken.None).ConfigureAwait(false);

        var body = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        return JsonSerializer.Deserialize(body, ResponsesJsonContext.Default.ResponseDeleteResult)
               ?? throw new FoundryLocalException($"Failed to deserialize delete result: {body[..Math.Min(body.Length, 200)]}", _logger);
    }

    private async Task<ResponseObject> CancelImplAsync(string responseId, CancellationToken? ct)
    {
        ValidateId(responseId, nameof(responseId));
        using var cancelResponse = await _httpClient.PostAsync(
            $"{_baseUrl}/v1/responses/{Uri.EscapeDataString(responseId)}/cancel",
            null, ct ?? CancellationToken.None).ConfigureAwait(false);
        await EnsureSuccessAsync(cancelResponse, ct ?? CancellationToken.None).ConfigureAwait(false);

        var body = await cancelResponse.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        return JsonSerializer.Deserialize(body, ResponsesJsonContext.Default.ResponseObject)
               ?? throw new FoundryLocalException($"Failed to deserialize response: {body[..Math.Min(body.Length, 200)]}", _logger);
    }

    private async Task<ResponseInputItemsList> GetInputItemsImplAsync(string responseId,
                                                                       CancellationToken? ct)
    {
        ValidateId(responseId, nameof(responseId));
        using var response = await _httpClient.GetAsync(
            $"{_baseUrl}/v1/responses/{Uri.EscapeDataString(responseId)}/input_items",
            ct ?? CancellationToken.None).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct ?? CancellationToken.None).ConfigureAwait(false);

        var body = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        return JsonSerializer.Deserialize(body, ResponsesJsonContext.Default.ResponseInputItemsList)
               ?? throw new FoundryLocalException($"Failed to deserialize input items: {body[..Math.Min(body.Length, 200)]}", _logger);
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    private ResponseCreateRequest BuildRequest(ResponseInput input, bool stream)
    {
        var model = _modelId;
        if (string.IsNullOrWhiteSpace(model))
        {
            throw new FoundryLocalException(
                "Model must be specified either in the constructor or via GetResponsesClient(modelId).");
        }

        // Merge order: model+input → settings defaults → per-call overrides (via Action)
        return new ResponseCreateRequest
        {
            Model = model,
            Input = input,
            Stream = stream,
            Instructions = Settings.Instructions,
            Temperature = Settings.Temperature,
            TopP = Settings.TopP,
            MaxOutputTokens = Settings.MaxOutputTokens,
            FrequencyPenalty = Settings.FrequencyPenalty,
            PresencePenalty = Settings.PresencePenalty,
            ToolChoice = Settings.ToolChoice,
            Truncation = Settings.Truncation,
            ParallelToolCalls = Settings.ParallelToolCalls,
            Store = Settings.Store,
            Metadata = Settings.Metadata,
            Reasoning = Settings.Reasoning,
            Text = Settings.Text,
            Seed = Settings.Seed,
        };
    }

    private static void ValidateId(string id, string paramName)
    {
        if (string.IsNullOrWhiteSpace(id))
        {
            throw new ArgumentException($"{paramName} must be a non-empty string.", paramName);
        }

        if (id.Length > 1024)
        {
            throw new ArgumentException($"{paramName} exceeds maximum length (1024).", paramName);
        }
    }

    private async Task EnsureSuccessAsync(HttpResponseMessage response, CancellationToken ct = default)
    {
        if (!response.IsSuccessStatusCode)
        {
            var errorBody = await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
            throw new FoundryLocalException(
                $"Responses API error ({(int)response.StatusCode}): {errorBody}", _logger);
        }
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                _httpClient.Dispose();
            }

            _disposed = true;
        }
    }

    public void Dispose()
    {
        Dispose(disposing: true);
        GC.SuppressFinalize(this);
    }
}

/// <summary>
/// Settings for the Responses API.
/// </summary>
public record ResponsesSettings
{
    /// <summary>System-level instructions to guide the model.</summary>
    public string? Instructions { get; set; }
    public float? Temperature { get; set; }
    public float? TopP { get; set; }
    public int? MaxOutputTokens { get; set; }
    public float? FrequencyPenalty { get; set; }
    public float? PresencePenalty { get; set; }
    public ResponseToolChoice? ToolChoice { get; set; }
    public string? Truncation { get; set; }
    public bool? ParallelToolCalls { get; set; }
    public bool? Store { get; set; }
    public Dictionary<string, string>? Metadata { get; set; }
    public ResponseReasoningConfig? Reasoning { get; set; }
    public ResponseTextConfig? Text { get; set; }
    public int? Seed { get; set; }
}
