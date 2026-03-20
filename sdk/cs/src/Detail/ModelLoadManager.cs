// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Collections.Generic;
using System.Text.Json;
using System.Threading;

using Microsoft.Extensions.Logging;

internal sealed class ModelLoadManager : IModelLoadManager, IDisposable
{
    private readonly Uri? _externalServiceUrl;
    private readonly HttpClient? _httpClient;
    private readonly ICoreInterop _coreInterop;
    private readonly ILogger _logger;

    internal ModelLoadManager(Uri? externalServiceUrl, ICoreInterop coreInterop, ILogger logger)
    {
        _externalServiceUrl = externalServiceUrl;
        _coreInterop = coreInterop;
        _logger = logger;

        if (_externalServiceUrl != null)
        {
            // We only have a single instance of ModelLoadManager so we don't need HttpClient to be static.
#pragma warning disable IDISP014 // Use a single instance of HttpClient.
            _httpClient = new HttpClient
            {
                BaseAddress = _externalServiceUrl,
            };
#pragma warning restore IDISP014 // Use a single instance of HttpClient

            // TODO: Wire in Config AppName here
            var userAgent = $"foundry-local-cs-sdk/{FoundryLocalManager.AssemblyVersion}";
            _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd(userAgent);
        }
    }

    public async Task LoadAsync(string modelId, CancellationToken? ct = null)
    {
        if (_externalServiceUrl != null)
        {
            await WebLoadModelAsync(modelId, ct).ConfigureAwait(false);
            return;
        }

        var request = new CoreInteropRequest { Params = new() { { "Model", modelId } } };
        var result = await _coreInterop.ExecuteCommandAsync("load_model", request, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error loading model {modelId}: {result.Error}");
        }

        // currently just a 'model loaded successfully' message
        _logger.LogInformation("Model {ModelId} loaded successfully: {Message}", modelId, result.Data);
    }

    public async Task UnloadAsync(string modelId, CancellationToken? ct = null)
    {
        if (_externalServiceUrl != null)
        {
            await WebUnloadModelAsync(modelId, ct).ConfigureAwait(false);
            return;
        }

        var request = new CoreInteropRequest { Params = new() { { "Model", modelId } } };
        var result = await _coreInterop.ExecuteCommandAsync("unload_model", request, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error unloading model {modelId}: {result.Error}");
        }

        _logger.LogInformation("Model {ModelId} unloaded successfully: {Message}", modelId, result.Data);
    }

    public async Task<string[]> ListLoadedModelsAsync(CancellationToken? ct = null)
    {
        if (_externalServiceUrl != null)
        {
            return await WebListLoadedModelAsync(ct).ConfigureAwait(false);
        }

        var result = await _coreInterop.ExecuteCommandAsync("list_loaded_models", null, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error listing loaded models: {result.Error}");
        }

        _logger.LogDebug("Loaded models json: {Data}", result.Data);

        var typeInfo = JsonSerializationContext.Default.StringArray;
        var modelList = JsonSerializer.Deserialize(result.Data!, typeInfo);

        return modelList ?? [];
    }

    private async Task<string[]> WebListLoadedModelAsync(CancellationToken? ct = null)
    {
        using var response = await _httpClient!.GetAsync("models/loaded", ct ?? CancellationToken.None)
                                               .ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            throw new FoundryLocalException($"Error listing loaded models from {_externalServiceUrl}: " +
                                                $"{response.ReasonPhrase}");
        }

        var content = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        _logger.LogDebug("Loaded models json from {WebService}: {Data}", _externalServiceUrl, content);
        var typeInfo = JsonSerializationContext.Default.StringArray;
        var modelList = JsonSerializer.Deserialize(content, typeInfo);
        return modelList ?? [];
    }

    private async Task WebLoadModelAsync(string modelId, CancellationToken? ct = null)
    {
        var queryParams = new Dictionary<string, string>
        {
            // { "timeout", ... }
        };

        var uriBuilder = new UriBuilder(_externalServiceUrl!)
        {
            Path = $"models/load/{modelId}",
            Query = string.Join("&", queryParams.Select(kvp =>
                $"{Uri.EscapeDataString(kvp.Key)}={Uri.EscapeDataString(kvp.Value)}"))
        };

        using var response = await _httpClient!.GetAsync(uriBuilder.Uri, ct ?? CancellationToken.None)
                                               .ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            throw new FoundryLocalException($"Error loading model {modelId} from {_externalServiceUrl}: " +
                                                $"{response.ReasonPhrase}");
        }

        var content = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        _logger.LogInformation("Model {ModelId} loaded successfully from {WebService}: {Message}",
                               modelId, _externalServiceUrl, content);
    }

    private async Task WebUnloadModelAsync(string modelId, CancellationToken? ct = null)
    {
        using var response = await _httpClient!.GetAsync(new Uri($"models/unload/{modelId}"),
                                                         ct ?? CancellationToken.None)
                                               .ConfigureAwait(false);

        // TODO: Do we need to handle a 400 (not found) explicitly or does that not provide any real value?
        if (!response.IsSuccessStatusCode)
        {
            throw new FoundryLocalException($"Error unloading model {modelId} from {_externalServiceUrl}: " +
                                                $"{response.ReasonPhrase}");
        }

        var content = await response.Content.ReadAsStringAsync(ct ?? CancellationToken.None).ConfigureAwait(false);
        _logger.LogInformation("Model {ModelId} loaded successfully from {WebService}: {Message}",
                               modelId, _externalServiceUrl, content);
    }

    public void Dispose()
    {
        _httpClient?.Dispose();
    }
}
