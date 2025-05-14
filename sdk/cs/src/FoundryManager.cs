// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

namespace FoundryLocal;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Text.Json;
using System.Net.Http.Json;
using System.Xml;

public class FoundryManager
{
    public static async Task<FoundryManager> StartModelAsync(string idOrAlias, CancellationToken ct = default)
    {
        var manager = new FoundryManager();
        await manager.start_service(ct);
        var modelInfo = await manager.get_model_info(idOrAlias, ct) ?? throw new InvalidOperationException($"Model {idOrAlias} not found in catalog.");
        await manager.download_model(modelInfo.Id, ct: ct);
        await manager.load_model(idOrAlias, ct: ct);
        return manager;
    }

    public FoundryManager()
    {
    }

    // Gets the service URI
    public Uri service_uri => _serviceUri ?? throw new InvalidOperationException("Service URI is not set. Call StartAsync() first.");

    // Get the endpoint for model control
    public Uri endpoint => new Uri(service_uri, "/v1");

    // Retrieves the current API key to use
    public string api_key { get; internal set; } = "OPENAI_API_KEY";

    // Sees if the service is already running
    public bool is_service_running => _serviceUri != null;

    public async Task start_service(CancellationToken ct = default)
    {
        if (_serviceUri == null)
        {
            _serviceUri = await EnsureServiceRunning(ct);
            _serviceClient = new HttpClient
            {
                BaseAddress = _serviceUri,
                Timeout = TimeSpan.FromSeconds(15)
            };
        }
    }

    public async Task<List<ModelInfo>> list_catalog_models(CancellationToken ct = default)
    {
        if (_catalogModels == null)
        {
            await start_service();
            var results = await _serviceClient!.GetAsync("/foundry/list", ct);
            var jsonResponse = await results.Content.ReadAsStringAsync(ct);
            var models = JsonSerializer.Deserialize(jsonResponse, ModelGenerationContext.Default.ListModelInfo);
            if (models == null)
            {
                return [];
            }

            _catalogModels = models;
        }

        return _catalogModels;
    }

    private async Task<Dictionary<string, ModelInfo>> _get_catalog_dict(CancellationToken ct = default)
    {
        if (_catalogDictionary == null)
        {
            var dict = new Dictionary<string, ModelInfo>(StringComparer.OrdinalIgnoreCase);
            var models = await list_catalog_models(ct);
            foreach (var model in models)
            {
                dict[model.Id] = model;
                dict[model.Alias] = model;
            }

            _catalogDictionary = dict;
        }

        return _catalogDictionary;
    }

    public void refresh_catalog()
    {
        _catalogModels = null;
        _catalogDictionary = null;
    }

    public async Task<ModelInfo?> get_model_info(string idOrAlias, CancellationToken ct = default)
    {
        var dictionary = await _get_catalog_dict(ct);

        ModelInfo? model;
        dictionary.TryGetValue(idOrAlias, out model);
        return model;
    }

    public async Task<string> get_cache_location(CancellationToken ct = default)
    {
        var response = await _serviceClient!.GetAsync("/openai/status", ct);
        var json = await response.Content.ReadAsStringAsync(ct);
        var jsonDocument = JsonDocument.Parse(json);
        return jsonDocument.RootElement.GetProperty("modelDirPath").GetString() ?? throw new InvalidOperationException("Model directory path not found in response.");
    }

    public async Task<List<ModelInfo>> _fetch_model_infos(IEnumerable<string> idsOrAliases, CancellationToken ct = default)
    {
        var modelInfos = new List<ModelInfo>();
        foreach (var idOrAlias in idsOrAliases)
        {
            var model = await get_model_info(idOrAlias, ct);
            if (model != null)
            {
                modelInfos.Add(model);
            }
        }
        return modelInfos;
    }

    public async Task<List<ModelInfo>> list_local_models(CancellationToken ct = default)
    {
        await start_service(ct);
        var results = await _serviceClient!.GetAsync("/openai/models", ct);
        var jsonResponse = await results.Content.ReadAsStringAsync(ct);
        var modelIds = JsonSerializer.Deserialize<string[]>(jsonResponse) ?? [];
        return await _fetch_model_infos(modelIds, ct);
    }

    public async Task<ModelInfo?> download_model(string idOrAlias, string? token = null, bool? force = false, CancellationToken ct = default)
    {
        var modelInfo = await get_model_info(idOrAlias, ct) ?? throw new InvalidOperationException($"Model {idOrAlias} not found in catalog.");
        var localModels = await list_local_models(ct);
        if (localModels.Where(_ => (_.Id == idOrAlias) || (_.Alias == idOrAlias)).Any() && !force.GetValueOrDefault(false))
        {
            return modelInfo;
        }

        var request = new DownloadRequest
        {
            Model = new DownloadRequest.ModelInfo
            {
                Name = modelInfo.Id,
                Uri = modelInfo.Uri,
                Path = await get_model_path(modelInfo.Id, ct) ?? throw new InvalidOperationException("Model path not found."),
                ProviderType = modelInfo.Provider,
                PromptTemplate = modelInfo.PromptTemplate ?? string.Empty
            },
            Token = api_key,
            IgnorePipeReport = true
        };
        var response = await _serviceClient!.PostAsJsonAsync("/foundry/download", request, ct);
        response.EnsureSuccessStatusCode();
        var responseBody = await response.Content.ReadAsStringAsync(ct);
        var jsonResponse = JsonDocument.Parse(responseBody);
        var statusCode = jsonResponse.RootElement.GetProperty("success").GetBoolean();
        if (!statusCode)
        {
            throw new InvalidOperationException($"Failed to download model: {jsonResponse}");
        }
        return modelInfo;
    }

    public async Task<ModelInfo> load_model(string idOrAlias, TimeSpan? timeout = null, CancellationToken ct = default)
    {
        var modelInfo = await get_model_info(idOrAlias, ct) ?? throw new InvalidOperationException($"Model {idOrAlias} not found in catalog.");
        var localModelInfo = await list_local_models(ct);
        if (!localModelInfo.Where(_ => (_.Id == idOrAlias) || (_.Alias == idOrAlias)).Any())
        {
            throw new InvalidOperationException($"Model {idOrAlias} not found in local models. Please download it first.");
        }

        var queryParams = new Dictionary<string, string>
        {
            { "timeout", (timeout ?? TimeSpan.FromMinutes(10)).TotalSeconds.ToString() }
        };
        if ((modelInfo.Runtime == ExecutionProvider.CUDA) || (modelInfo.Runtime == ExecutionProvider.WEBGPU))
        {
            bool hasCudaSupport = localModelInfo.Any(m => m.Runtime == ExecutionProvider.CUDA);
            queryParams["ep"] = hasCudaSupport ? "CUDA" : modelInfo.Runtime.ToString();
        }

        var uriBuilder = new UriBuilder(service_uri);
        uriBuilder.Path = $"/openai/load/{modelInfo.Id}";
        uriBuilder.Query = string.Join("&", queryParams.Select(kvp => $"{Uri.EscapeDataString(kvp.Key)}={Uri.EscapeDataString(kvp.Value)}"));

        var response = await _serviceClient!.GetAsync(uriBuilder.Uri, ct);
        response.EnsureSuccessStatusCode();

        return modelInfo;
    }

    public async Task<List<ModelInfo>> list_loaded_models()
    {
        var response = await _serviceClient!.GetAsync(new Uri(service_uri, "/openai/loadedmodels"));
        response.EnsureSuccessStatusCode();
        var names = await response.Content.ReadFromJsonAsync<string[]>() ?? throw new InvalidOperationException("Failed to read loaded models.");
        return await _fetch_model_infos(names, CancellationToken.None);
    }

    public async Task unload_model(string idOrAlias, CancellationToken ct = default)
    {
        var modelInfo = await get_model_info(idOrAlias, ct) ?? throw new InvalidOperationException($"Model {idOrAlias} not found in catalog.");
        var response = await _serviceClient!.GetAsync($"/openai/unload/{modelInfo.Id}");
        response.EnsureSuccessStatusCode();
    }

    private static async Task<string?> get_model_path(string assetId, CancellationToken ct = default)
    {
        var baseAddress = $"https://eastus.api.azureml.ms/modelregistry/v1.0/registry/models/nonazureaccount?assetId={assetId}";
        var client = new HttpClient{Timeout = TimeSpan.FromSeconds(15)};
        var response = await client.GetAsync(baseAddress, ct);
        response.EnsureSuccessStatusCode();
        var jsonDocument = JsonDocument.Parse(await response.Content.ReadAsStringAsync(ct));
        var blob_sas_url = new Uri(jsonDocument.RootElement.GetProperty("blobSasUri").GetString() ?? throw new InvalidOperationException("Blob SAS URI not found in response."));
                
        var list_url = new UriBuilder(blob_sas_url)
        {
            Query = $"restype=container&comp=list&delimiter=/&{blob_sas_url.Query}"
        }.Uri;

        var list_response = await client.GetAsync(list_url, ct);
        list_response.EnsureSuccessStatusCode();
        var root = new XmlDocument();

        // Find the first .//BlobPrefix, find its child element Name, and read its text
        var blobPrefix = root.SelectSingleNode(".//BlobPrefix")?.SelectSingleNode("Name")?.InnerText.Trim('/');
        return blobPrefix ?? throw new InvalidOperationException("Blob prefix not found in response.");
    }

    private Uri? _serviceUri;
    private HttpClient? _serviceClient;
    private List<ModelInfo>? _catalogModels;
    private Dictionary<string, ModelInfo>? _catalogDictionary;

    const string _azureMlServiceUri = "https://foundry.azure.com/api/v1";

    static async Task<Uri?> EnsureServiceRunning(CancellationToken ct = default)
    {
        var startResult = await InvokeFoundry("service start", ct);

        var started = Regex.Match(startResult, "Started on (http://localhost:\\d+)");
        if (started.Success)
        {
            return new Uri(started.Groups[1].Value);
        }

        var alreadyRunning = Regex.Match(startResult, "Failed to start service, port (\\d+) already in use");
        if (alreadyRunning.Success)
        {
            return new Uri($"http://localhost:{alreadyRunning.Groups[1].Value}");
        }

        return await StatusEndpoint(ct);
    }

    static async Task<Uri?> StatusEndpoint(CancellationToken ct = default)
    {
        var statusResult = await InvokeFoundry("service status", ct);
        var status = Regex.Match(statusResult, "is running on (http://.*)\\s+");
        if (status.Success)
        {
            var uri = new Uri(status.Groups[1].Value);
            var builder = new UriBuilder() { Scheme = uri.Scheme, Host = uri.Host, Port = uri.Port };
            return builder.Uri;
        }
        return status.Success ? new Uri(status.Groups[1].Value) : null;
    }
    
    static async Task<string> InvokeFoundry(string args, CancellationToken ct = default)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo
        {
            FileName = "d:\\foundry-local\\artifacts\\bin\\Foundry.Local.Client\\debug_win-x64\\foundry.exe", // "foundry",
            Arguments = String.Join(" ", args),
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = false
        };

        using Process process = new Process
        {
            StartInfo = startInfo
        };

        process.Start();

        return await Task.Run(() =>
        {
            var response = process.StandardOutput.ReadToEnd();
            response += process.StandardError.ReadToEnd();
            process.WaitForExit();
            return response;
        }, ct);
    }
}
