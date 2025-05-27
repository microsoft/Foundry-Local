﻿// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Http.Json;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

[JsonConverter(typeof(JsonStringEnumConverter<DeviceType>))]

public enum DeviceType
{
    CPU,
    GPU,
    NPU,
    Invalid
}

[JsonConverter(typeof(JsonStringEnumConverter<ExecutionProvider>))]
public enum ExecutionProvider
{
    Invalid,
    CPUExecutionProvider,
    WebGpuExecutionProvider,
    CUDAExecutionProvider,
    QNNExecutionProvider
}

public partial class FoundryLocalManager : IDisposable, IAsyncDisposable
{
    private Uri? _serviceUri;
    private HttpClient? _serviceClient;
    private List<ModelInfo>? _catalogModels;
    private Dictionary<string, ModelInfo>? _catalogDictionary;
    private readonly Dictionary<ExecutionProvider, int> _priorityMap;

    // Gets the service URI
    public Uri ServiceUri => _serviceUri ?? throw new InvalidOperationException("Service URI is not set. Call StartServiceAsync() first.");

    // Get the endpoint for model control
    public Uri Endpoint => new(ServiceUri, "/v1");

    // Retrieves the current API key to use
    public string ApiKey { get; internal set; } = "OPENAI_API_KEY";

    // Sees if the service is already running
    public bool IsServiceRunning => _serviceUri != null;

    public FoundryLocalManager()
    {
        var preferredOrder = new List<ExecutionProvider>
        {
            ExecutionProvider.QNNExecutionProvider,
            ExecutionProvider.CUDAExecutionProvider,
            ExecutionProvider.CPUExecutionProvider,
            ExecutionProvider.WebGpuExecutionProvider
        };

        if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            preferredOrder.Remove(ExecutionProvider.CPUExecutionProvider);
            preferredOrder.Add(ExecutionProvider.CPUExecutionProvider);
        }

        _priorityMap = preferredOrder
            .Select((provider, index) => new { provider, index })
            .ToDictionary(x => x.provider, x => x.index);
    }

    public static async Task<FoundryLocalManager> StartModelAsync(string aliasOrModelId, CancellationToken ct = default)
    {
        var manager = new FoundryLocalManager();
        try
        {
            await manager.StartServiceAsync(ct);
            var modelInfo = await manager.GetModelInfoAsync(aliasOrModelId, ct)
                ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
            await manager.DownloadModelAsync(modelInfo.ModelId, ct: ct);
            await manager.LoadModelAsync(aliasOrModelId, ct: ct);
            return manager;
        }
        catch
        {
            manager.Dispose();
            throw;
        }
    }

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

    public async Task StopServiceAsync(CancellationToken ct = default)
    {
        if (_serviceUri != null)
        {
            await InvokeFoundry("service stop", ct);
            _serviceUri = null;
            _serviceClient = null;
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
            await StartServiceAsync(ct);
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

    public void RefreshCatalog()
    {
        _catalogModels = null;
        _catalogDictionary = null;
    }

    public async Task<ModelInfo?> GetModelInfoAsync(string aliasorModelId, CancellationToken ct = default)
    {
        var dictionary = await GetCatalogDictAsync(ct);

        dictionary.TryGetValue(aliasorModelId, out ModelInfo? model);
        return model;
    }

    public async Task<string> GetCacheLocationAsync(CancellationToken ct = default)
    {
        await StartServiceAsync(ct);
        var response = await _serviceClient!.GetAsync("/openai/status", ct);
        var json = await response.Content.ReadAsStringAsync(ct);
        var jsonDocument = JsonDocument.Parse(json);
        return jsonDocument.RootElement.GetProperty("modelDirPath").GetString()
            ?? throw new InvalidOperationException("Model directory path not found in response.");
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
        var modelInfo = await GetModelInfoAsync(aliasOrModelId, ct)
            ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        var localModels = await ListCachedModelsAsync(ct);
        if (localModels.Any(_ => _.ModelId == aliasOrModelId || _.Alias == aliasOrModelId) && !force.GetValueOrDefault(false))
        {
            return modelInfo;
        }

        var request = new DownloadRequest
        {
            Model = new DownloadRequest.ModelInfo
            {
                Name = modelInfo.ModelId,
                Uri = modelInfo.Uri,
                ProviderType = modelInfo.ProviderType + "Local",
                PromptTemplate = modelInfo.PromptTemplate
            },
            Token = token ?? "",
            IgnorePipeReport = true
        };

        var response = await _serviceClient!.PostAsJsonAsync("/openai/download", request, ct);
        response.EnsureSuccessStatusCode();
        var responseBody = await response.Content.ReadAsStringAsync(ct);

        // Find the last '{' to get the start of the JSON object
        var jsonStart = responseBody.LastIndexOf('{');
        if (jsonStart == -1)
        {
            throw new InvalidOperationException("No JSON object found in response.");
        }

        var jsonPart = responseBody[jsonStart..];

        // Parse the JSON part
        using var jsonDoc = JsonDocument.Parse(jsonPart);
        var success = jsonDoc.RootElement.GetProperty("success").GetBoolean();
        var errorMessage = jsonDoc.RootElement.GetProperty("errorMessage").GetString();

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
        if (!localModelInfo.Any(_ => _.ModelId == aliasOrModelId || _.Alias == aliasOrModelId))
        {
            throw new InvalidOperationException($"Model {aliasOrModelId} not found in local models. Please download it first.");
        }

        var queryParams = new Dictionary<string, string>
        {
            { "timeout", (timeout ?? TimeSpan.FromMinutes(10)).TotalSeconds.ToString() }
        };

        if (modelInfo.Runtime.DeviceType == DeviceType.GPU)
        {
            var hasCudaSupport = localModelInfo.Any(m => m.Runtime.ExecutionProvider == ExecutionProvider.CUDAExecutionProvider);
            queryParams["ep"] = hasCudaSupport ? "CUDA" : modelInfo.Runtime.ToString();
        }

        var uriBuilder = new UriBuilder(ServiceUri)
        {
            Path = $"/openai/load/{modelInfo.ModelId}",
            Query = string.Join("&", queryParams.Select(kvp => $"{Uri.EscapeDataString(kvp.Key)}={Uri.EscapeDataString(kvp.Value)}"))
        };

        var response = await _serviceClient!.GetAsync(uriBuilder.Uri, ct);
        response.EnsureSuccessStatusCode();

        return modelInfo;
    }

    public async Task<List<ModelInfo>> ListLoadedModelsAsync(CancellationToken ct = default)
    {
        var response = await _serviceClient!.GetAsync(new Uri(ServiceUri, "/openai/loadedmodels"), ct);
        response.EnsureSuccessStatusCode();
        var names = await response.Content.ReadFromJsonAsync<string[]>(ct)
            ?? throw new InvalidOperationException("Failed to read loaded models.");
        return await FetchModelInfosAsync(names, CancellationToken.None);
    }

    public async Task UnloadModelAsync(string aliasOrModelId, CancellationToken ct = default)
    {
        var modelInfo = await GetModelInfoAsync(aliasOrModelId, ct)
            ?? throw new InvalidOperationException($"Model {aliasOrModelId} not found in catalog.");
        var response = await _serviceClient!.GetAsync($"/openai/unload/{modelInfo.ModelId}?force=true", ct);

        response.EnsureSuccessStatusCode();
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

    private async Task<Dictionary<string, ModelInfo>> GetCatalogDictAsync(CancellationToken ct = default)
    {
        if (_catalogDictionary == null)
        {
            var dict = new Dictionary<string, ModelInfo>(StringComparer.OrdinalIgnoreCase);
            var models = await ListCatalogModelsAsync(ct);
            foreach (var model in models)
            {
                dict[model.ModelId] = model;

                if (!string.IsNullOrWhiteSpace(model.Alias))
                {
                    if (!dict.TryGetValue(model.Alias, out var existing))
                    {
                        dict[model.Alias] = model;
                    }
                    else
                    {
                        var currentPriority = _priorityMap.TryGetValue(model.Runtime.ExecutionProvider, out var cp) ? cp : int.MaxValue;
                        var existingPriority = _priorityMap.TryGetValue(existing.Runtime.ExecutionProvider, out var ep) ? ep : int.MaxValue;

                        if (currentPriority < existingPriority)
                        {
                            dict[model.Alias] = model;
                        }
                    }
                }
            }

            _catalogDictionary = dict;
        }

        return _catalogDictionary;
    }

    private static async Task<Uri?> EnsureServiceRunning(CancellationToken ct = default)
    {
        var startInfo = new ProcessStartInfo
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

        using var process = new Process
        {
            StartInfo = startInfo
        };

        process.Start();

        await process.WaitForExitAsync(ct);

        return await StatusEndpoint(ct);
    }

    private static async Task<Uri?> StatusEndpoint(CancellationToken ct = default)
    {
        var statusResult = await InvokeFoundry("service status", ct);
        var status = MyRegex().Match(statusResult);
        if (status.Success)
        {
            var uri = new Uri(status.Groups[1].Value);
            var builder = new UriBuilder() { Scheme = uri.Scheme, Host = uri.Host, Port = uri.Port };
            return builder.Uri;
        }
        return status.Success ? new Uri(status.Groups[1].Value) : null;
    }

    private static async Task<string> InvokeFoundry(string args, CancellationToken ct = default)
    {
        var startInfo = new ProcessStartInfo
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

        using Process process = Process.Start(startInfo)
            ?? throw new InvalidOperationException("Failed to start the foundry process");

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

    [GeneratedRegex("is running on (http://.*)\\s+")]
    private static partial Regex MyRegex();
}
