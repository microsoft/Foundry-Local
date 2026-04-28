// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.OpenAI.Responses;

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

    public TruncationValue? Truncation { get; set; }

    /// <summary>
    /// Server-side storage of responses. When <c>null</c> (default), the field is omitted
    /// from the request and the server applies its default. Set to <c>true</c> to persist
    /// responses for later retrieval via <see cref="OpenAIResponsesClient.GetAsync"/>,
    /// <see cref="OpenAIResponsesClient.ListAsync"/>, and <see cref="OpenAIResponsesClient.DeleteAsync"/>.
    /// </summary>
    public bool? Store { get; set; }

    public Dictionary<string, string>? Metadata { get; set; }

    public ReasoningConfig? Reasoning { get; set; }

    public TextConfig? Text { get; set; }

    public string? User { get; set; }

    internal void ApplyTo(ResponseCreateRequest request)
    {
        request.Instructions ??= Instructions;
        request.Temperature ??= Temperature;
        request.TopP ??= TopP;
        request.MaxOutputTokens ??= MaxOutputTokens;
        request.ParallelToolCalls ??= ParallelToolCalls;
        request.Truncation ??= Truncation;
        request.Store ??= Store;
        request.Metadata ??= Metadata;
        request.Reasoning ??= Reasoning;
        request.Text ??= Text;
        request.User ??= User;
    }
}

/// <summary>
/// HTTP client for the OpenAI Responses API served by Foundry Local.
/// Uses <see cref="HttpClient"/> directly — no FFI/native interop.
/// </summary>
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP007:Don't dispose injected", Justification = "Client only disposes HttpClient when it owns it (ownsClient flag tracked at construction).")]
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP008:Don't assign member with injected and created disposables", Justification = "Client owns HttpClient when constructed without one.")]
[System.Diagnostics.CodeAnalysis.SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP014:Use a single instance of HttpClient", Justification = "Short-lived per-client HttpClient matches SDK pattern; callers share via FoundryLocalManager.")]
public sealed class OpenAIResponsesClient : IDisposable
{
    private readonly HttpClient _httpClient;
    private readonly string _baseUrl;
    private readonly string? _modelId;
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
        : this(CreateDefaultHttpClient(), baseUrl, modelId, ownsClient: true)
    {
    }

    internal OpenAIResponsesClient(HttpClient httpClient, string baseUrl, string? modelId = null, bool ownsClient = true)
    {
        ArgumentNullException.ThrowIfNull(httpClient);

        if (string.IsNullOrWhiteSpace(baseUrl))
        {
            throw new ArgumentException("baseUrl must be non-empty.", nameof(baseUrl));
        }

        _httpClient = httpClient;
        _baseUrl = baseUrl.TrimEnd('/');
        _modelId = modelId;
        _ownsClient = ownsClient;
    }

    // Streaming SSE connections stay open until the server finishes producing events,
    // which can exceed HttpClient's 100s default. Disable the built-in timeout and let
    // callers enforce request-scoped deadlines via CancellationToken.
    private static HttpClient CreateDefaultHttpClient() => new() { Timeout = Timeout.InfiniteTimeSpan };

    private readonly bool _ownsClient;

    // -----------------------------------------------------------------------------------------------------------------
    // Create (non-streaming)
    // -----------------------------------------------------------------------------------------------------------------

    public Task<ResponseObject> CreateAsync(string input, CancellationToken ct = default)
        => CreateAsync(input, configure: null, ct);

    public Task<ResponseObject> CreateAsync(string input, Action<ResponseCreateRequest>? configure, CancellationToken ct = default)
    {
        ValidateStringInput(input);
        return CreateAsync(BuildRequest(input, configure), ct);
    }

    public Task<ResponseObject> CreateAsync(List<ResponseItem> input, CancellationToken ct = default)
        => CreateAsync(input, configure: null, ct);

    public Task<ResponseObject> CreateAsync(List<ResponseItem> input, Action<ResponseCreateRequest>? configure, CancellationToken ct = default)
    {
        ValidateListInput(input);
        return CreateAsync(BuildRequest(input, configure), ct);
    }

    /// <summary>Submit a raw request object.</summary>
    public async Task<ResponseObject> CreateAsync(ResponseCreateRequest request, CancellationToken ct = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        request.Stream = false;
        using var content = SerializeRequest(request);
        using var response = await _httpClient.PostAsync(Url("/v1/responses"), content, ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);

        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var parsed = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.ResponseObject, ct)
            .ConfigureAwait(false);
        return parsed ?? throw new FoundryLocalException("Server returned an empty response body.");
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Create (streaming)
    // -----------------------------------------------------------------------------------------------------------------

    public IAsyncEnumerable<StreamingEvent> CreateStreamingAsync(string input, CancellationToken ct = default)
        => CreateStreamingAsync(input, configure: null, ct);

    public IAsyncEnumerable<StreamingEvent> CreateStreamingAsync(string input, Action<ResponseCreateRequest>? configure, CancellationToken ct = default)
    {
        ValidateStringInput(input);
        return CreateStreamingAsync(BuildRequest(input, configure), ct);
    }

    public IAsyncEnumerable<StreamingEvent> CreateStreamingAsync(List<ResponseItem> input, CancellationToken ct = default)
        => CreateStreamingAsync(input, configure: null, ct);

    public IAsyncEnumerable<StreamingEvent> CreateStreamingAsync(List<ResponseItem> input, Action<ResponseCreateRequest>? configure, CancellationToken ct = default)
    {
        ValidateListInput(input);
        return CreateStreamingAsync(BuildRequest(input, configure), ct);
    }

    /// <summary>Stream events for a raw request object.</summary>
    public async IAsyncEnumerable<StreamingEvent> CreateStreamingAsync(ResponseCreateRequest request, [EnumeratorCancellation] CancellationToken ct = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        request.Stream = true;

        var channel = Channel.CreateUnbounded<StreamingEvent>(new UnboundedChannelOptions
        {
            SingleReader = true,
            SingleWriter = true,
        });

        var pumpTask = Task.Run(() => PumpSseAsync(request, channel.Writer, ct), ct);

        await foreach (var ev in channel.Reader.ReadAllAsync(ct).ConfigureAwait(false))
        {
            yield return ev;
        }

        // Surface any exception that happened on the background pump.
        await pumpTask.ConfigureAwait(false);
    }

    private async Task PumpSseAsync(ResponseCreateRequest request, ChannelWriter<StreamingEvent> writer, CancellationToken ct)
    {
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Post, Url("/v1/responses"))
            {
                Content = SerializeRequest(request),
            };
            req.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("text/event-stream"));

            using var response = await _httpClient.SendAsync(req, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false);
            await EnsureSuccessAsync(response, ct).ConfigureAwait(false);

            await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
            using var reader = new StreamReader(stream, Encoding.UTF8);

            var dataBuilder = new StringBuilder();
            while (!reader.EndOfStream)
            {
                ct.ThrowIfCancellationRequested();
                var line = await reader.ReadLineAsync(ct).ConfigureAwait(false);
                if (line == null)
                {
                    break;
                }

                if (line.Length == 0)
                {
                    // End of an SSE event block.
                    if (dataBuilder.Length > 0)
                    {
                        var payload = dataBuilder.ToString();
                        dataBuilder.Clear();

                        if (payload == "[DONE]")
                        {
                            break;
                        }

                        var ev = ParseStreamingEvent(payload);
                        if (ev != null)
                        {
                            await writer.WriteAsync(ev, ct).ConfigureAwait(false);
                        }
                    }

                    continue;
                }

                if (line.StartsWith("data:", StringComparison.Ordinal))
                {
                    var data = line.Length > 5 && line[5] == ' ' ? line.Substring(6) : line.Substring(5);
                    if (dataBuilder.Length > 0)
                    {
                        dataBuilder.Append('\n');
                    }

                    dataBuilder.Append(data);
                }

                // `event:`, `id:`, and comment lines (`:`) are ignored — the type is in the JSON payload.
            }

            // If stream ended without a blank-line terminator, flush any pending data.
            if (dataBuilder.Length > 0)
            {
                var payload = dataBuilder.ToString();
                if (payload != "[DONE]")
                {
                    var ev = ParseStreamingEvent(payload);
                    if (ev != null)
                    {
                        await writer.WriteAsync(ev, ct).ConfigureAwait(false);
                    }
                }
            }

            writer.TryComplete();
        }
        catch (OperationCanceledException)
        {
            writer.TryComplete();
            throw;
        }
        catch (Exception ex)
        {
            writer.TryComplete(ex);
        }
    }

    internal static StreamingEvent? ParseStreamingEvent(string payload)
    {
        try
        {
            return JsonSerializer.Deserialize(payload, ResponsesSerializationContext.Default.StreamingEvent);
        }
        catch (JsonException)
        {
            return null;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    // CRUD
    // -----------------------------------------------------------------------------------------------------------------

    public async Task<ResponseObject> GetAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);
        using var response = await _httpClient.GetAsync(Url($"/v1/responses/{responseId}"), ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);
        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var result = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.ResponseObject, ct).ConfigureAwait(false);
        return result ?? throw new FoundryLocalException("Server returned an empty response body.");
    }

    public async Task<DeleteResponseResult> DeleteAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);
        using var response = await _httpClient.DeleteAsync(Url($"/v1/responses/{responseId}"), ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);
        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var result = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.DeleteResponseResult, ct).ConfigureAwait(false);
        return result ?? throw new FoundryLocalException("Server returned an empty delete response body.");
    }

    public async Task<ResponseObject> CancelAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);
        using var content = new StringContent(string.Empty, Encoding.UTF8, "application/json");
        using var response = await _httpClient.PostAsync(Url($"/v1/responses/{responseId}/cancel"), content, ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);
        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var result = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.ResponseObject, ct).ConfigureAwait(false);
        return result ?? throw new FoundryLocalException("Server returned an empty cancel response body.");
    }

    public async Task<InputItemsListResponse> GetInputItemsAsync(string responseId, CancellationToken ct = default)
    {
        ValidateId(responseId);
        using var response = await _httpClient.GetAsync(Url($"/v1/responses/{responseId}/input_items"), ct).ConfigureAwait(false);
        await EnsureSuccessAsync(response, ct).ConfigureAwait(false);
        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var result = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.InputItemsListResponse, ct).ConfigureAwait(false);
        return result ?? throw new FoundryLocalException("Server returned an empty input-items response body.");
    }

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
        await using var stream = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        var result = await JsonSerializer.DeserializeAsync(stream, ResponsesSerializationContext.Default.ListResponsesResult, ct).ConfigureAwait(false);
        return result ?? throw new FoundryLocalException("Server returned an empty list response body.");
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------------------------------------------

    private string Url(string relative) => _baseUrl + relative;

    private ResponseCreateRequest BuildRequest(string input, Action<ResponseCreateRequest>? configure)
    {
        var request = new ResponseCreateRequest
        {
            Model = _modelId ?? string.Empty,
            Input = input,
        };
        Settings.ApplyTo(request);
        configure?.Invoke(request);
        EnsureModel(request);
        ValidateTools(request);
        ValidateImageContents(request);
        return request;
    }

    private ResponseCreateRequest BuildRequest(List<ResponseItem> input, Action<ResponseCreateRequest>? configure)
    {
        var request = new ResponseCreateRequest
        {
            Model = _modelId ?? string.Empty,
            Input = input,
        };
        Settings.ApplyTo(request);
        configure?.Invoke(request);
        EnsureModel(request);
        ValidateTools(request);
        ValidateImageContents(request);
        return request;
    }

    private static void ValidateImageContents(ResponseCreateRequest request)
    {
        var items = request.Input?.Items;
        if (items is null)
        {
            return;
        }
        foreach (var item in items)
        {
            if (item is MessageItem msg && msg.Content?.Parts is { } parts)
            {
                foreach (var part in parts)
                {
                    if (part is InputImageContent img)
                    {
                        img.Validate();
                    }
                }
            }
        }
    }

    private static void EnsureModel(ResponseCreateRequest request)
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

    private static void ValidateListInput(List<ResponseItem> input)
    {
        ArgumentNullException.ThrowIfNull(input);

        if (input.Count == 0)
        {
            throw new ArgumentException("Input list must contain at least one item.", nameof(input));
        }
    }

    private static void ValidateId(string id)
    {
        if (string.IsNullOrWhiteSpace(id))
        {
            throw new ArgumentException("Response id must be non-empty.", nameof(id));
        }
    }

    private static void ValidateTools(ResponseCreateRequest request)
    {
        if (request.Tools == null)
        {
            return;
        }

        foreach (var tool in request.Tools)
        {
            if (string.IsNullOrWhiteSpace(tool.Name))
            {
                throw new ArgumentException("Tool definition name must be non-empty.");
            }
        }
    }

    private static StringContent SerializeRequest(ResponseCreateRequest request)
    {
        var json = JsonSerializer.Serialize(request, ResponsesSerializationContext.Default.ResponseCreateRequest);
        return new StringContent(json, Encoding.UTF8, "application/json");
    }

    private static async Task EnsureSuccessAsync(HttpResponseMessage response, CancellationToken ct)
    {
        if (response.IsSuccessStatusCode)
        {
            return;
        }

        string body = string.Empty;
        try
        {
            body = await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
        }
        catch
        {
            // ignore read failure — we still raise for status.
        }

        // Try to parse the OpenAI-style error envelope for a nicer message.
        string? serverMessage = null;
        if (!string.IsNullOrWhiteSpace(body))
        {
            try
            {
                var parsed = JsonSerializer.Deserialize(body, ResponsesSerializationContext.Default.ApiErrorResponse);
                serverMessage = parsed?.Error?.Message;
            }
            catch (JsonException)
            {
                // ignore parse failure — fall through to raw body.
            }
        }

        var message = serverMessage ?? (string.IsNullOrWhiteSpace(body) ? response.ReasonPhrase : body);
        throw new FoundryLocalException($"Responses API request failed ({(int)response.StatusCode} {response.ReasonPhrase}): {message}");
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        if (_ownsClient)
        {
            _httpClient.Dispose();
        }
    }
}
