// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

namespace Microsoft.AI.Foundry.Local;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Text.Json;
using System.Net.Http.Json;
using System.Xml;
using System.Text;

public class FoundryManager : IDisposable, IAsyncDisposable
{
    public static async Task<FoundryManager> StartModelAsync(string aliasOrModelId, CancellationToken ct = default)
    {
        var manager = new FoundryManager();
        await manager.StartServiceAsync(ct);
        var modelInfo = await manager.GetModelInfoAsync(aliasOrModelId, ct) ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        await manager.DownloadModelAsync(modelInfo.ModelId, ct: ct);
        await manager.LoadModelAsync(aliasOrModelId, ct: ct);
        return manager;
    }

    public FoundryManager()
    {
    }

    // Gets the service URI
    public Uri ServiceUri => _serviceUri ?? throw new InvalidOperationException("Service URI is not set. Call StartAsync() first.");

    // Get the endpoint for model control
    public Uri Endpoint => new Uri(ServiceUri, "/v1");

    // Retrieves the current API key to use
    public string ApiKey { get; internal set; } = "OPENAI_API_KEY";

    // Sees if the service is already running
    public bool IsServiceRunning => _serviceUri != null;

    public async Task StartServiceAsync(CancellationToken ct = default)
    {
        if (_serviceUri == null)
        {
            _serviceUri = await EnsureServiceRunning(ct);
            _serviceClient = new HttpClient
            {
                BaseAddress = _serviceUri,
                // set the timeout to 2 hours (for downloading large models)
                Timeout = TimeSpan.FromSeconds(7200)
            };
        }
    }

    public void Dispose()
    {
        _serviceClient?.Dispose();
        GC.SuppressFinalize(this); // Ensures that the finalizer is not called for this object.
    }

    public async ValueTask DisposeAsync()
    {
        if (_serviceClient is IAsyncDisposable asyncDisposable)
        {
            await asyncDisposable.DisposeAsync();
        }
        else
        {
            _serviceClient?.Dispose();
        }
        GC.SuppressFinalize(this); // Ensures that the finalizer is not called for this object.
    }

    public async Task<List<ModelInfo>> ListCatalogModelsAsync(CancellationToken ct = default)
    {
        if (_catalogModels == null)
        {
            await StartServiceAsync();
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

    private async Task<Dictionary<string, ModelInfo>> GetCatalogDictAsync(CancellationToken ct = default)
    {
        if (_catalogDictionary == null)
        {
            var dict = new Dictionary<string, ModelInfo>(StringComparer.OrdinalIgnoreCase);
            var models = await ListCatalogModelsAsync(ct);
            foreach (var model in models)
            {
                dict[model.ModelId] = model;
                dict[model.Alias] = model;
            }

            _catalogDictionary = dict;
        }

        return _catalogDictionary;
    }

    public void RefreshCatalog()
    {
        _catalogModels = null;
        _catalogDictionary = null;
    }

    public async Task<ModelInfo?> GetModelInfoAsync(string aliasorModelId, CancellationToken ct = default)
    {
        var dictionary = await GetCatalogDictAsync(ct);

        ModelInfo? model;
        dictionary.TryGetValue(aliasorModelId, out model);
        return model;
    }

    public async Task<string> GetCacheLocationAsync(CancellationToken ct = default)
    {
        await StartServiceAsync(ct);
        var response = await _serviceClient!.GetAsync("/openai/status", ct);
        var json = await response.Content.ReadAsStringAsync(ct);
        var jsonDocument = JsonDocument.Parse(json);
        return jsonDocument.RootElement.GetProperty("modelDirPath").GetString() ?? throw new InvalidOperationException("Model directory path not found in response.");
    }

    private async Task<List<ModelInfo>> FetchModelInfosAsync(IEnumerable<string> aliasesOrModelIds, CancellationToken ct = default)
    {
        var modelInfos = new List<ModelInfo>();
        foreach (var idOrAlias in aliasesOrModelIds)
        {
            var model = await GetModelInfoAsync(idOrAlias, ct);
            if (model != null)
            {
                modelInfos.Add(model);
            }
        }
        return modelInfos;
    }

    public async Task<List<ModelInfo>> ListCachedModelsAsync(CancellationToken ct = default)
    {
        await StartServiceAsync(ct);
        var results = await _serviceClient!.GetAsync("/openai/models", ct);
        var jsonResponse = await results.Content.ReadAsStringAsync(ct);
        var modelIds = JsonSerializer.Deserialize<string[]>(jsonResponse) ?? [];
        return await FetchModelInfosAsync(modelIds, ct);
    }

    public async Task<ModelInfo?> DownloadModelAsync(string aliasOrModelId, string? token = null, bool? force = false, CancellationToken ct = default)
    {
        var modelInfo = await GetModelInfoAsync(aliasOrModelId, ct) ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        var localModels = await ListCachedModelsAsync(ct);
        if (localModels.Where(_ => _.ModelId == aliasOrModelId || _.Alias == aliasOrModelId).Any() && !force.GetValueOrDefault(false))
        {
            return modelInfo;
        }

        var request = new DownloadRequest
        {
            Model = new DownloadRequest.ModelInfo
            {
                Name = modelInfo.ModelId,
                Uri = modelInfo.Uri,
                ProviderType = modelInfo.ProviderType+"Local",
                PromptTemplate = modelInfo.PromptTemplate
            },
            Token = token ?? "",
            IgnorePipeReport = true
        };

        var response = await _serviceClient!.PostAsJsonAsync("/openai/download", request, ct);
        response.EnsureSuccessStatusCode();
        var responseBody = await response.Content.ReadAsStringAsync(ct);

        // Find the last '{' to get the start of the JSON object
        int jsonStart = responseBody.LastIndexOf('{');
        if (jsonStart == -1)
            throw new InvalidOperationException("No JSON object found in response.");

        string jsonPart = responseBody.Substring(jsonStart);

        // Parse the JSON part
        using var jsonDoc = JsonDocument.Parse(jsonPart);
        bool success = jsonDoc.RootElement.GetProperty("success").GetBoolean();
        string? errorMessage = jsonDoc.RootElement.GetProperty("errorMessage").GetString();

        if (!success)
        {
            throw new InvalidOperationException($"Failed to download model: {errorMessage}");
        }

        return modelInfo;
    }

    public async Task<ModelInfo> LoadModelAsync(string aliasOrModelId, TimeSpan? timeout = null, CancellationToken ct = default)
    {
        var modelInfo = await GetModelInfoAsync(aliasOrModelId, ct) ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        var localModelInfo = await ListCachedModelsAsync(ct);
        if (!localModelInfo.Where(_ => _.ModelId == aliasOrModelId || _.Alias == aliasOrModelId).Any())
        {
            throw new InvalidOperationException($"Model {aliasOrModelId} not found in local models. Please download it first.");
        }

        var queryParams = new Dictionary<string, string>
        {
            { "timeout", (timeout ?? TimeSpan.FromMinutes(10)).TotalSeconds.ToString() }
        };
        // TODO: Make gpu an enumeration
        if (modelInfo.Runtime.DeviceType == "gpu")
        {
            // TODO: Make CUDAExecutionProvider an enumeration
            bool hasCudaSupport = localModelInfo.Any(m => m.Runtime.ExecutionProvider == "CUDAExecutionProvider");
            queryParams["ep"] = hasCudaSupport ? "CUDA" : modelInfo.Runtime.ToString();
        }

        var uriBuilder = new UriBuilder(ServiceUri);
        uriBuilder.Path = $"/openai/load/{modelInfo.ModelId}";
        uriBuilder.Query = string.Join("&", queryParams.Select(kvp => $"{Uri.EscapeDataString(kvp.Key)}={Uri.EscapeDataString(kvp.Value)}"));

        var response = await _serviceClient!.GetAsync(uriBuilder.Uri, ct);
        response.EnsureSuccessStatusCode();

        return modelInfo;
    }

    public async Task<List<ModelInfo>> ListLoadedModelsAsync(CancellationToken ct = default)
    {
        var response = await _serviceClient!.GetAsync(new Uri(ServiceUri, "/openai/loadedmodels"), ct);
        response.EnsureSuccessStatusCode();
        var names = await response.Content.ReadFromJsonAsync<string[]>(ct) ?? throw new InvalidOperationException("Failed to read loaded models.");
        return await FetchModelInfosAsync(names, CancellationToken.None);
    }

    public async Task StopServiceAsync(CancellationToken ct = default)
    {
        await InvokeFoundry("service stop", ct);
        _serviceUri = null;
        _serviceClient = null;
    }

    public async Task UnloadModelAsync(string aliasOrModelId, CancellationToken ct = default)
    {
        var modelInfo = await GetModelInfoAsync(aliasOrModelId, ct) ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        var response = await _serviceClient!.GetAsync($"/openai/unload/{modelInfo.ModelId}?force=true", ct);

        response.EnsureSuccessStatusCode();
    }

    private Uri? _serviceUri;
    private HttpClient? _serviceClient;
    private List<ModelInfo>? _catalogModels;
    private Dictionary<string, ModelInfo>? _catalogDictionary;

    const string _azureMlServiceUri = "https://foundry.azure.com/api/v1";

    static async Task<Uri?> EnsureServiceRunning(CancellationToken ct = default)
    {
        // TODO: This commented out code would hang in the scenario where the service is not running.
        // suspect it is because the reading from stdout in InvokeFoundry is blocking.

        //var startResult = await InvokeFoundry("service start", ct);

        //var started = Regex.Match(startResult, "Started on (http://localhost:\\d+)");
        //if (started.Success)
        //{
        //    return new Uri(started.Groups[1].Value);
        //}

        //var alreadyRunning = Regex.Match(startResult, "Failed to start service, port (\\d+) already in use");
        //if (alreadyRunning.Success)
        //{
        //    return new Uri($"http://localhost:{alreadyRunning.Groups[1].Value}");
        //}
        ProcessStartInfo startInfo = new ProcessStartInfo
        {
            FileName = @"foundry",
            Arguments = "service start",
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        // This is a workaround for the issue where the
        // environment variable DOTNET_ENVIRONMENT is set to "Development" by default.
        startInfo.Environment["DOTNET_ENVIRONMENT"] = null;


        using Process process = new Process
        {
            StartInfo = startInfo
        };

        process.Start();

        await process.WaitForExitAsync(ct);

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
            FileName = @"foundry",
            Arguments = args,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        };

        // This is a workaround for the issue where the
        // environment variable DOTNET_ENVIRONMENT is set to "Development" by default.
        startInfo.Environment["DOTNET_ENVIRONMENT"] = null;

        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Failed to start the foundry process");

        var output = new StringBuilder();

        process.OutputDataReceived += (sender, e) =>
        {
            if (e.Data is not null)
            {
                output.AppendLine(e.Data);
            }
        };
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        await process.WaitForExitAsync(ct);

        return output.ToString();
    }
}
