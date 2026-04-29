// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

#pragma warning disable OPENAI001 // OpenAI Responses APIs are experimental in the official OpenAI package.

namespace Microsoft.AI.Foundry.Local;

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.OpenAI.Responses;
using OfficialResponses = global::OpenAI.Responses;
using System.ClientModel;

/// <summary>
/// Default-value container for <see cref="OpenAIResponsesClient"/>.
/// Any non-null settings here are applied to every request before the
/// per-call <c>configure</c> callback runs.
/// </summary>
public class ResponsesClientSettings
{
    public string? Instructions { get; set; }

    public float? Temperature { get; set; }

    public float? TopP { get; set; }

    public int? MaxOutputTokens { get; set; }

    public bool? ParallelToolCalls { get; set; }

    public OfficialResponses.ResponseTruncationMode? Truncation { get; set; }

    /// <summary>
    /// Server-side storage of responses. When <c>null</c> (default), the field is omitted
    /// from the request and the server applies its default. Set to <c>true</c> to persist
    /// responses for later retrieval via <see cref="OpenAIResponsesClient.GetAsync"/>,
    /// <see cref="OpenAIResponsesClient.ListAsync"/>, and <see cref="OpenAIResponsesClient.DeleteAsync"/>.
    /// </summary>
    public bool? Store { get; set; }

    public Dictionary<string, string>? Metadata { get; set; }

    public OfficialResponses.ResponseReasoningOptions? Reasoning { get; set; }

    public OfficialResponses.ResponseTextOptions? Text { get; set; }

    public string? User { get; set; }

    internal void ApplyTo(OfficialResponses.CreateResponseOptions request)
    {
        request.Instructions ??= Instructions;
        request.Temperature ??= Temperature;
        request.TopP ??= TopP;
        request.MaxOutputTokenCount ??= MaxOutputTokens;
        request.ParallelToolCallsEnabled ??= ParallelToolCalls;
        request.TruncationMode ??= Truncation;
        request.StoredOutputEnabled ??= Store;
        request.ReasoningOptions ??= Reasoning;
        request.TextOptions ??= Text;
        request.EndUserId ??= User;

        if (Metadata is not null)
        {
            foreach (var (key, value) in Metadata)
            {
                request.Metadata.TryAdd(key, value);
            }
        }
    }
}

/// <summary>
/// Client for the OpenAI Responses API served by Foundry Local.
/// Uses the official <see cref="OfficialResponses.ResponsesClient"/> for standard Responses endpoints and a small HTTP
/// shim for Foundry Local's list-responses extension.
/// </summary>
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP007:Don't dispose injected", Justification = "Client only disposes HttpClient when it owns it (ownsClient flag tracked at construction).")]
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP008:Don't assign member with injected and created disposables", Justification = "Client owns HttpClient when constructed without one.")]
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP014:Use a single instance of HttpClient", Justification = "Short-lived per-client HttpClient matches SDK pattern; callers share via FoundryLocalManager.")]
public sealed class OpenAIResponsesClient : IDisposable
{
    private const string LocalApiKey = "foundry-local";

    private readonly HttpClient _httpClient;
    private readonly OfficialResponses.ResponsesClient _responsesClient;
    private readonly string _baseUrl;
    private readonly string? _modelId;
    private readonly bool _ownsClient;
    private bool _disposed;

    /// <summary>Default settings applied to every request.</summary>
    public ResponsesClientSettings Settings { get; } = new();

    /// <summary>Gets the underlying base URL (without trailing slash).</summary>
    public string BaseUrl => _baseUrl;

    /// <summary>Gets the default model id (if any) supplied at construction.</summary>
    public string? ModelId => _modelId;

    /// <summary>
    /// Create a new client for the given base URL.
    /// </summary>
    /// <param name="baseUrl">Base URL of the Foundry Local service (e.g., http://localhost:5273).</param>
    /// <param name="modelId">Default model id to use when callers do not set one explicitly.</param>
    public OpenAIResponsesClient(string baseUrl, string? modelId = null)
        : this(CreateDefaultHttpClient(), CreateOfficialClient(baseUrl), baseUrl, modelId, ownsClient: true)
    {
    }

    internal OpenAIResponsesClient(
        HttpClient httpClient,
        OfficialResponses.ResponsesClient responsesClient,
        string baseUrl,
        string? modelId = null,
        bool ownsClient = true)
    {
        ArgumentNullException.ThrowIfNull(httpClient);
        ArgumentNullException.ThrowIfNull(responsesClient);

        if (string.IsNullOrWhiteSpace(baseUrl))
        {
            throw new ArgumentException("baseUrl must be non-empty.", nameof(baseUrl));
        }

        _httpClient = httpClient;
        _responsesClient = responsesClient;
        _baseUrl = baseUrl.TrimEnd('/');
        _modelId = modelId;
        _ownsClient = ownsClient;
    }

    // Kept for tests that only exercise Foundry Local extension endpoints.
    internal OpenAIResponsesClient(HttpClient httpClient, string baseUrl, string? modelId = null, bool ownsClient = true)
        : this(httpClient, CreateOfficialClient(baseUrl), baseUrl, modelId, ownsClient)
    {
    }

    // Foundry Local responses streaming may exceed HttpClient's 100s default. Disable the built-in
    // timeout for the list-extension client and let callers use CancellationToken for deadlines.
    private static HttpClient CreateDefaultHttpClient() => new() { Timeout = Timeout.InfiniteTimeSpan };

    private static OfficialResponses.ResponsesClient CreateOfficialClient(string baseUrl)
    {
        if (string.IsNullOrWhiteSpace(baseUrl))
        {
            throw new ArgumentException("baseUrl must be non-empty.", nameof(baseUrl));
        }

        var endpoint = new Uri(baseUrl.TrimEnd('/') + "/v1");
        return new OfficialResponses.ResponsesClient(
            new ApiKeyCredential(LocalApiKey),
            new global::OpenAI.OpenAIClientOptions { Endpoint = endpoint });
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Create (non-streaming)
    // -----------------------------------------------------------------------------------------------------------------

    public Task<OfficialResponses.ResponseResult> CreateAsync(string input, CancellationToken ct = default)
        => CreateAsync(input, configure: null, ct);

    public Task<OfficialResponses.ResponseResult> CreateAsync(string input, Action<OfficialResponses.CreateResponseOptions>? configure, CancellationToken ct = default)
    {
        ValidateStringInput(input);
        return CreateAsync(BuildRequest([OfficialResponses.ResponseItem.CreateUserMessageItem(input)], configure), ct);
    }

    public Task<OfficialResponses.ResponseResult> CreateAsync(IEnumerable<OfficialResponses.ResponseItem> input, CancellationToken ct = default)
        => CreateAsync(input, configure: null, ct);

    public Task<OfficialResponses.ResponseResult> CreateAsync(IEnumerable<OfficialResponses.ResponseItem> input, Action<OfficialResponses.CreateResponseOptions>? configure, CancellationToken ct = default)
    {
        ValidateListInput(input);
        return CreateAsync(BuildRequest(input, configure), ct);
    }

    /// <summary>Submit a raw official OpenAI request object.</summary>
    public async Task<OfficialResponses.ResponseResult> CreateAsync(OfficialResponses.CreateResponseOptions request, CancellationToken ct = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        request.StreamingEnabled = false;
        EnsureModel(request);

        try
        {
            var result = await _responsesClient.CreateResponseAsync(request, ct).ConfigureAwait(false);
            return result.Value;
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Create (streaming)
    // -----------------------------------------------------------------------------------------------------------------

    public IAsyncEnumerable<OfficialResponses.StreamingResponseUpdate> CreateStreamingAsync(string input, CancellationToken ct = default)
        => CreateStreamingAsync(input, configure: null, ct);

    public IAsyncEnumerable<OfficialResponses.StreamingResponseUpdate> CreateStreamingAsync(string input, Action<OfficialResponses.CreateResponseOptions>? configure, CancellationToken ct = default)
    {
        ValidateStringInput(input);
        return CreateStreamingAsync(BuildRequest([OfficialResponses.ResponseItem.CreateUserMessageItem(input)], configure), ct);
    }

    public IAsyncEnumerable<OfficialResponses.StreamingResponseUpdate> CreateStreamingAsync(IEnumerable<OfficialResponses.ResponseItem> input, CancellationToken ct = default)
        => CreateStreamingAsync(input, configure: null, ct);

    public IAsyncEnumerable<OfficialResponses.StreamingResponseUpdate> CreateStreamingAsync(IEnumerable<OfficialResponses.ResponseItem> input, Action<OfficialResponses.CreateResponseOptions>? configure, CancellationToken ct = default)
    {
        ValidateListInput(input);
        return CreateStreamingAsync(BuildRequest(input, configure), ct);
    }

    /// <summary>Stream events for a raw official OpenAI request object.</summary>
    public async IAsyncEnumerable<OfficialResponses.StreamingResponseUpdate> CreateStreamingAsync(
        OfficialResponses.CreateResponseOptions request,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken ct = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        request.StreamingEnabled = true;
        EnsureModel(request);

        AsyncCollectionResult<OfficialResponses.StreamingResponseUpdate> updates;
        try
        {
            updates = _responsesClient.CreateResponseStreamingAsync(request, ct);
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }

        await foreach (var update in updates)
        {
            yield return update;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    // CRUD
    // -----------------------------------------------------------------------------------------------------------------

    public async Task<OfficialResponses.ResponseResult> GetAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);

        try
        {
            var result = await _responsesClient.GetResponseAsync(responseId, ct).ConfigureAwait(false);
            return result.Value;
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }
    }

    public async Task<OfficialResponses.ResponseDeletionResult> DeleteAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);

        try
        {
            var result = await _responsesClient.DeleteResponseAsync(responseId, ct).ConfigureAwait(false);
            return result.Value;
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }
    }

    public async Task<OfficialResponses.ResponseResult> CancelAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);

        try
        {
            var result = await _responsesClient.CancelResponseAsync(responseId, ct).ConfigureAwait(false);
            return result.Value;
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }
    }

    public async Task<OfficialResponses.ResponseItemCollectionPage> GetInputItemsAsync(
        string responseId,
        int? limit = null,
        string? order = null,
        string? after = null,
        string? before = null,
        CancellationToken ct = default)
    {
        ValidateId(responseId);

        var options = new OfficialResponses.ResponseItemCollectionOptions(responseId)
        {
            PageSizeLimit = limit,
            AfterId = after,
            BeforeId = before,
        };
        if (!string.IsNullOrWhiteSpace(order))
        {
            options.Order = new OfficialResponses.ResponseItemCollectionOrder(order);
        }

        try
        {
            var result = await _responsesClient.GetResponseInputItemCollectionPageAsync(options, ct).ConfigureAwait(false);
            return result.Value;
        }
        catch (ClientResultException ex)
        {
            throw ToFoundryLocalException(ex);
        }
    }

    /// <summary>
    /// Lists stored responses using Foundry Local's extension endpoint. The official OpenAI .NET
    /// Responses client does not currently expose a typed list-responses method.
    /// </summary>
    public async Task<ListResponsesResult> ListAsync(
        int? limit = null,
        string? order = null,
        string? after = null,
        CancellationToken ct = default)
    {
        var query = new List<string>();
        if (limit.HasValue)
        {
            query.Add($"limit={limit.Value}");
        }
        if (!string.IsNullOrWhiteSpace(order))
        {
            query.Add($"order={Uri.EscapeDataString(order)}");
        }
        if (!string.IsNullOrWhiteSpace(after))
        {
            query.Add($"after={Uri.EscapeDataString(after)}");
        }

        var url = Url("/v1/responses") + (query.Count > 0 ? "?" + string.Join("&", query) : "");
        using var response = await _httpClient.GetAsync(url, ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);

        var json = await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
        return ListResponsesResult.FromJson(json);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------------------------------------------

    private string Url(string relative) => _baseUrl + relative;

    private OfficialResponses.CreateResponseOptions BuildRequest(IEnumerable<OfficialResponses.ResponseItem> input, Action<OfficialResponses.CreateResponseOptions>? configure)
    {
        var request = new OfficialResponses.CreateResponseOptions(_modelId ?? string.Empty, input);
        Settings.ApplyTo(request);
        configure?.Invoke(request);
        EnsureModel(request);
        return request;
    }

    private static void EnsureModel(OfficialResponses.CreateResponseOptions request)
    {
        if (string.IsNullOrWhiteSpace(request.Model))
        {
            throw new ArgumentException("A model id must be provided via constructor or configure callback.");
        }
    }

    private static void ValidateStringInput(string input)
    {
        ArgumentNullException.ThrowIfNull(input);

        if (input.Length == 0)
        {
            throw new ArgumentException("Input string must be non-empty.", nameof(input));
        }
    }

    private static void ValidateListInput(IEnumerable<OfficialResponses.ResponseItem> input)
    {
        ArgumentNullException.ThrowIfNull(input);

        using var enumerator = input.GetEnumerator();
        if (!enumerator.MoveNext())
        {
            throw new ArgumentException("Input list must contain at least one item.", nameof(input));
        }
    }

    private static void ValidateId(string id)
    {
        if (string.IsNullOrWhiteSpace(id))
        {
            throw new ArgumentException("responseId must be non-empty.", nameof(id));
        }
    }

    private static async Task EnsureSuccessAsync(HttpResponseMessage response, CancellationToken ct)
    {
        if (response.IsSuccessStatusCode)
        {
            return;
        }

        var body = await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
        var message = TryReadErrorMessage(body) ?? $"{(int)response.StatusCode} {response.ReasonPhrase}";
        throw new FoundryLocalException($"Responses API request failed: {message}");
    }

    private static string? TryReadErrorMessage(string body)
    {
        if (string.IsNullOrWhiteSpace(body))
        {
            return null;
        }

        try
        {
            using var doc = JsonDocument.Parse(body);
            if (doc.RootElement.TryGetProperty("error", out var error)
                && error.TryGetProperty("message", out var message))
            {
                return message.GetString();
            }
        }
        catch (JsonException)
        {
            return null;
        }

        return null;
    }

    private static FoundryLocalException ToFoundryLocalException(ClientResultException ex)
    {
        var message = string.IsNullOrWhiteSpace(ex.Message) ? $"HTTP {ex.Status}" : ex.Message;
        return new FoundryLocalException($"Responses API request failed: {message}", ex);
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        if (_ownsClient)
        {
            _httpClient.Dispose();
        }

        _disposed = true;
    }
}

#pragma warning restore OPENAI001
