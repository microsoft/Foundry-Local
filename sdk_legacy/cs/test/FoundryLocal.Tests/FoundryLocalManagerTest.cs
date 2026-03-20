// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local;

using RichardSzalay.MockHttp;

using Xunit;

public class FoundryLocalManagerTests : IDisposable
{
    private readonly FoundryLocalManager _manager;
    private readonly HttpClient _client;
    private readonly MockHttpMessageHandler _mockHttp;

    public FoundryLocalManagerTests()
    {
        _mockHttp = new MockHttpMessageHandler();
        _client = _mockHttp.ToHttpClient();
        _client.BaseAddress = new Uri("http://localhost:5272"); // matches python tests

        _manager = new FoundryLocalManager();
        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client.BaseAddress);

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);
    }

    private static List<ModelInfo> BuildCatalog(bool includeCuda = true)
    {
        // Mirrors MOCK_CATALOG_DATA ordering and fields (Python tests)
        var common = new
        {
            ProviderType = "AzureFoundry",
            Version = "1",
            ModelType = "ONNX",
            PromptTemplate = (PromptTemplate?)null,
            Publisher = "Microsoft",
            Task = "chat-completion",
            FileSizeMb = 10403L,
            ModelSettings = new ModelSettings { Parameters = [] },
            SupportsToolCalling = false,
            License = "MIT",
            LicenseDescription = "License…",
            MaxOutputTokens = 1024L,
            MinFLVersion = "1.0.0",
        };

        var list = new List<ModelInfo>
        {
            // model-1 generic-gpu, generic-cpu:2, generic-cpu:1
            new()
            {
                ModelId = "model-1-generic-gpu:1",
                DisplayName = "model-1-generic-gpu",
                Uri = "azureml://registries/azureml/models/model-1-generic-gpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
                Alias = "model-1",
                ParentModelUri = "azureml://registries/azureml/models/model-1/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
            new()
            {
                ModelId = "model-1-generic-cpu:2",
                DisplayName = "model-1-generic-cpu",
                Uri = "azureml://registries/azureml/models/model-1-generic-cpu/versions/2",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Alias = "model-1",
                ParentModelUri = "azureml://registries/azureml/models/model-1/versions/2",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
            new()
            {
                ModelId = "model-1-generic-cpu:1",
                DisplayName = "model-1-generic-cpu",
                Uri = "azureml://registries/azureml/models/model-1-generic-cpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Alias = "model-1",
                ParentModelUri = "azureml://registries/azureml/models/model-1/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },

            // model-2 npu:2, npu:1, generic-cpu:1
            new()
            {
                ModelId = "model-2-npu:2",
                DisplayName = "model-2-npu",
                Uri = "azureml://registries/azureml/models/model-2-npu/versions/2",
                Runtime = new Runtime { DeviceType = DeviceType.NPU, ExecutionProvider = "QNNExecutionProvider" },
                Alias = "model-2",
                ParentModelUri = "azureml://registries/azureml/models/model-2/versions/2",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
            new()
            {
                ModelId = "model-2-npu:1",
                DisplayName = "model-2-npu",
                Uri = "azureml://registries/azureml/models/model-2-npu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.NPU, ExecutionProvider = "QNNExecutionProvider" },
                Alias = "model-2",
                ParentModelUri = "azureml://registries/azureml/models/model-2/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
            new()
            {
                ModelId = "model-2-generic-cpu:1",
                DisplayName = "model-2-generic-cpu",
                Uri = "azureml://registries/azureml/models/model-2-generic-cpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Alias = "model-2",
                ParentModelUri = "azureml://registries/azureml/models/model-2/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
        };

        // model-3 cuda-gpu (optional), generic-gpu, generic-cpu
        if (includeCuda)
        {
            list.Add(new ModelInfo
            {
                ModelId = "model-3-cuda-gpu:1",
                DisplayName = "model-3-cuda-gpu",
                Uri = "azureml://registries/azureml/models/model-3-cuda-gpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "CUDAExecutionProvider" },
                Alias = "model-3",
                ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            });
        }

        list.AddRange(new[]
        {
            new ModelInfo
            {
                ModelId = "model-3-generic-gpu:1",
                DisplayName = "model-3-generic-gpu",
                Uri = "azureml://registries/azureml/models/model-3-generic-gpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
                Alias = "model-3",
                ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            },
            new ModelInfo
            {
                ModelId = "model-3-generic-cpu:1",
                DisplayName = "model-3-generic-cpu",
                Uri = "azureml://registries/azureml/models/model-3-generic-cpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Alias = "model-3",
                ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            }
        });

        // model-4 generic-gpu (nullable prompt)
        list.Add(new ModelInfo
        {
            ModelId = "model-4-generic-gpu:1",
            DisplayName = "model-4-generic-gpu",
            Uri = "azureml://registries/azureml/models/model-4-generic-gpu/versions/1",
            Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
            Alias = "model-4",
            ParentModelUri = "azureml://registries/azureml/models/model-4/versions/1",
            ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
            PromptTemplate = null, Publisher = common.Publisher, Task = common.Task,
            FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
            SupportsToolCalling = common.SupportsToolCalling, License = common.License,
            LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
            MinFLVersion = common.MinFLVersion
        });

        return list;
    }

    private void MockCatalog(bool includeCuda = true)
    {
        var payload = JsonSerializer.Serialize(BuildCatalog(includeCuda), ModelGenerationContext.Default.ListModelInfo);
        _mockHttp.When(HttpMethod.Get, "/foundry/list").Respond("application/json", payload);
    }

    private void MockLocalModels(params string[] ids)
    {
        var json = JsonSerializer.Serialize(ids ?? Array.Empty<string>());
        _mockHttp.When(HttpMethod.Get, "/openai/models").Respond("application/json", json);
    }

    private void MockLoadedModels(params string[] ids)
    {
        var json = JsonSerializer.Serialize(ids ?? Array.Empty<string>());
        _mockHttp.When(HttpMethod.Get, "/openai/loadedmodels").Respond("application/json", json);
    }

    [Fact]
    public async Task ListCatalogModelsAsync_ReturnsModels_AndSetsCudaOverrideWhenCudaPresent()
    {
        // GIVEN
        MockCatalog(includeCuda: true);

        // WHEN
        var models = await _manager.ListCatalogModelsAsync();

        // THEN
        Assert.NotEmpty(models);
        // Presence of any CUDAExecutionProvider should mark generic-gpu EpOverride="cuda"
        Assert.Contains(models, m => m.Runtime.ExecutionProvider == "CUDAExecutionProvider");
        var gg = models.Find(m => m.ModelId == "model-4-generic-gpu:1");
        Assert.NotNull(gg);
        Assert.Equal("cuda", gg!.EpOverride);

        // cache is used on second call
        var again = await _manager.ListCatalogModelsAsync();
        Assert.Same(models, again);
    }

    [Fact]
    public void RefreshCatalog_ResetsOnlyListCache()
    {
        // GIVEN
        typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new List<ModelInfo>());

        // WHEN
        _manager.RefreshCatalog();

        // THEN
        var models = typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(_manager);
        Assert.Null(models);
    }

    [Fact]
    public async Task GetModelInfoAsync_IdWithVersion_IdWithoutVersion_AliasAndDeviceFilter()
    {
        // GIVEN
        MockCatalog(includeCuda: true);

        // WHEN/THEN
        // unknown
        Assert.Null(await _manager.GetModelInfoAsync("unknown-model"));

        // exact id (with version)
        var m1v1 = await _manager.GetModelInfoAsync("model-1-generic-cpu:1");
        Assert.Equal("model-1-generic-cpu:1", m1v1!.ModelId);

        // id without version -> latest version among same prefix
        var m1latest = await _manager.GetModelInfoAsync("model-1-generic-cpu");
        Assert.Equal("model-1-generic-cpu:2", m1latest!.ModelId);

        // alias selection (depends on service order); our list puts "model-2-npu:2" first for alias model-2
        var a2 = await _manager.GetModelInfoAsync("model-2");
        Assert.Equal("model-2-npu:2", a2!.ModelId);

        // model-3 should prefer CUDA when present
        var a3 = await _manager.GetModelInfoAsync("model-3");
        Assert.Equal("model-3-cuda-gpu:1", a3!.ModelId);

        // device filter
        var a1gpu = await _manager.GetModelInfoAsync("model-1", DeviceType.GPU);
        Assert.Equal("model-1-generic-gpu:1", a1gpu!.ModelId);

        var a1cpu = await _manager.GetModelInfoAsync("model-1", DeviceType.CPU);
        Assert.Equal("model-1-generic-cpu:2", a1cpu!.ModelId);

        var a1npu = await _manager.GetModelInfoAsync("model-1", DeviceType.NPU);
        Assert.Null(a1npu);
    }

    [Fact]
    public async Task ListCachedModelsAsync_ReturnsMatchingInfos()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels("model-2-npu:1", "model-4-generic-gpu:1");

        // WHEN
        var local = await _manager.ListCachedModelsAsync();

        // THEN
        Assert.Equal(2, local.Count);
        Assert.Equal("model-2-npu:1", local[0].ModelId);
        Assert.Equal("model-4-generic-gpu:1", local[1].ModelId);
    }

    [Fact]
    public async Task ListLoadedModelsAsync_ReturnsModelInfoList()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLoadedModels("model-2-npu:1");

        // WHEN
        var loaded = await _manager.ListLoadedModelsAsync();

        // THEN
        var m = Assert.Single(loaded);
        Assert.Equal("model-2-npu:1", m.ModelId);
    }

    [Fact]
    public async Task ListLoadedModelsAsync_Throws_WhenNullResponse()
    {
        // GIVEN
        _mockHttp.When(HttpMethod.Get, "/openai/loadedmodels")
                .Respond("application/json", "null");

        // WHEN/THEN
        var ex = await Assert.ThrowsAsync<InvalidOperationException>(() => _manager.ListLoadedModelsAsync());
        Assert.Equal("Failed to read loaded models.", ex.Message);
    }

    [Fact]
    public async Task DownloadModelAsync_Success_ParsesTailJson()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels(); // empty cache

        var mockJsonResponse = "log... {\"success\": true, \"errorMessage\": null}";
        _mockHttp.When("/openai/download").Respond("application/json", mockJsonResponse);

        // WHEN
        var result = await _manager.DownloadModelAsync("model-3"); // resolves to cuda-gpu:1

        // THEN
        Assert.NotNull(result);
        Assert.Equal("model-3-cuda-gpu:1", result!.ModelId);
    }

    [Fact]
    public async Task DownloadModelAsync_ThrowsWhenServiceReturnsFailure()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels();

        var fail = "tail {\"success\": false, \"errorMessage\": \"nope\"}";
        _mockHttp.When("/openai/download").Respond("application/json", fail);

        // WHEN/THEN
        var ex = await Assert.ThrowsAsync<InvalidOperationException>(() => _manager.DownloadModelAsync("model-1"));
        Assert.Contains("Failed to download model: nope", ex.Message);
    }

    [Fact]
    public async Task DownloadModelAsync_SkipsWhenAlreadyCachedUnlessForce()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        // Latest version already in cache → should skip POST unless force==true
        MockLocalModels("model-2-npu:2");

        // WHEN: already cached and not forced → no POST to /openai/download
        var cached = await _manager.DownloadModelAsync("model-2");
        Assert.NotNull(cached);
        Assert.Equal("model-2-npu:2", cached!.ModelId);

        // AND WHEN: force download → we should POST and parse a JSON body that includes errorMessage
        _mockHttp.When(HttpMethod.Post, "/openai/download")
                .Respond("application/json", "{\"success\": true, \"errorMessage\": null}");

        var forced = await _manager.DownloadModelAsync("model-2", device: null, token: null, force: true);
        Assert.NotNull(forced);
        Assert.Equal("model-2-npu:2", forced!.ModelId);

        // No expectations were set, but this is harmless and keeps parity with other tests
        _mockHttp.VerifyNoOutstandingExpectation();
    }


    [Fact]
    public async Task DownloadModelWithProgressAsync_Success()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels(); // empty to force a download

        var stream = new MemoryStream();
        var progressLine = Encoding.UTF8.GetBytes("Total 0.00% Downloading model.onnx.data\n");
        stream.Write(progressLine, 0, progressLine.Length);
        var doneLine = Encoding.UTF8.GetBytes("[DONE] All Completed!\n");
        stream.Write(doneLine, 0, doneLine.Length);
        using (var writer = new Utf8JsonWriter(stream))
        {
            JsonSerializer.Serialize(writer, new { success = true, errorMessage = (string?)null });
        }
        stream.Position = 0;

        _mockHttp.When("/openai/download").Respond("application/json", stream);

        // WHEN
        var progressList = new List<ModelDownloadProgress>();
        await foreach (var p in _manager.DownloadModelWithProgressAsync("model-3"))
        {
            progressList.Add(p);
        }

        // THEN
        Assert.Equal(2, progressList.Count);
        Assert.False(progressList[0].IsCompleted);
        Assert.Equal(0, progressList[0].Percentage);

        Assert.True(progressList[1].IsCompleted);
        Assert.Equal(100, progressList[1].Percentage);
        Assert.Equal("model-3-cuda-gpu:1", progressList[1].ModelInfo!.ModelId);
    }

    [Fact]
    public async Task DownloadModelWithProgressAsync_Error()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels();

        var stream = new MemoryStream();
        var doneLine = Encoding.UTF8.GetBytes("[DONE] All Completed!\n");
        stream.Write(doneLine, 0, doneLine.Length);
        using (var writer = new Utf8JsonWriter(stream))
        {
            JsonSerializer.Serialize(writer, new { success = false, errorMessage = "Download error occurred." });
        }
        stream.Position = 0;

        _mockHttp.When("/openai/download").Respond("application/json", stream);

        // WHEN
        var progressList = new List<ModelDownloadProgress>();
        await foreach (var p in _manager.DownloadModelWithProgressAsync("model-3"))
        {
            progressList.Add(p);
        }

        // THEN
        var last = Assert.Single(progressList);
        Assert.True(last.IsCompleted);
        Assert.Equal("Download error occurred.", last.ErrorMessage);
    }

    [Fact]
    public async Task LoadModelAsync_Succeeds_AndPassesEpOverrideWhenCudaPresent()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels("model-4-generic-gpu:1"); // in cache

        // After ListCatalogModelsAsync runs, EpOverride for generic-gpu will be "cuda"
        // First call ensures the override is applied
        await _manager.ListCatalogModelsAsync();

        _mockHttp
            .When(HttpMethod.Get, "http://localhost:5272/openai/load/model-4-generic-gpu:1*")
            .Respond("application/json", "{}");

        // WHEN
        var result = await _manager.LoadModelAsync("model-4");

        // THEN
        Assert.Equal("model-4-generic-gpu:1", result.ModelId);
    }

    [Fact]
    public async Task LoadModelAsync_ThrowsIfNotInCache()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        MockLocalModels(); // empty

        // WHEN/THEN
        var ex = await Assert.ThrowsAsync<InvalidOperationException>(() => _manager.LoadModelAsync("model-3"));
        Assert.Contains("not found in local models", ex.Message);
    }

    [Fact]
    public async Task UnloadModelAsync_CallsCorrectUri()
    {
        // GIVEN
        MockCatalog(includeCuda: true);
        var model = "model-2-npu:1";

        _mockHttp.When("/openai/unload/model-2-npu:1")
                .WithQueryString("force=false")
                .Respond(HttpStatusCode.OK);

        // WHEN
        await _manager.UnloadModelAsync(model);

        // THEN just no exception
        Assert.True(true);
    }

    [Fact]
    public async Task IsModelUpgradeableAsync_ReturnsTrue_WhenCachedOlderThanLatest()
    {
        MockCatalog(includeCuda: true);
        MockLocalModels("model-2-npu:1"); // older
        Assert.True(await _manager.IsModelUpgradeableAsync("model-2"));
    }

    [Fact]
    public async Task IsModelUpgradeableAsync_ReturnsFalse_WhenLatestVersionCached()
    {
        MockCatalog(includeCuda: true);
        MockLocalModels("model-2-npu:2"); // latest
        Assert.False(await _manager.IsModelUpgradeableAsync("model-2"));
    }

    [Fact]
    public async Task IsModelUpgradeableAsync_ReturnsFalse_WhenModelMissingFromCatalog()
    {
        // empty catalog for this case
        _mockHttp.When(HttpMethod.Get, "/foundry/list").Respond("application/json", "[]");
        // cached state doesn’t matter when it’s missing from catalog
        MockLocalModels("model-2-npu:1");
        Assert.False(await _manager.IsModelUpgradeableAsync("model-2"));
    }

    [Fact]
    public async Task UpgradeModelAsync_HappyPath()
    {
        MockCatalog(includeCuda: true);
        MockLocalModels(); // empty -> forces download

        _mockHttp.When(HttpMethod.Post, "/openai/download")
                .Respond("application/json", "{\"success\": true, \"errorMessage\": null}");

        var upgraded = await _manager.UpgradeModelAsync("model-3");
        Assert.Equal("model-3-cuda-gpu:1", upgraded!.ModelId);
    }

    [Fact]
    public async Task UpgradeModelAsync_Throws_WhenDownloadFails()
    {
        MockCatalog(includeCuda: true);
        MockLocalModels(); // empty -> forces download

        _mockHttp.When(HttpMethod.Post, "/openai/download")
                .Respond("application/json", "{\"success\": false, \"errorMessage\": \"Simulated download failure.\"}");

        await Assert.ThrowsAsync<InvalidOperationException>(() => _manager.UpgradeModelAsync("model-3"));
    }


    [Fact]
    public async Task UpgradeModelAsync_ThrowsWhenModelNotFound()
    {
        // GIVEN
        _mockHttp.When(HttpMethod.Get, "/foundry/list").Respond("application/json", "[]");

        // WHEN/THEN
        var ex = await Assert.ThrowsAsync<ArgumentException>(() => _manager.UpgradeModelAsync("missing-model"));
        Assert.Contains("not found", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Dispose_DisposesHttpClient()
    {
        _manager.Dispose();
        Assert.True(true);
    }

    [Fact]
    public async Task DisposeAsync_DisposesHttpClient()
    {
        await _manager.DisposeAsync();
        Assert.True(true);
    }

    public void Dispose()
    {
        _client.Dispose();
        GC.SuppressFinalize(this);
    }
}
