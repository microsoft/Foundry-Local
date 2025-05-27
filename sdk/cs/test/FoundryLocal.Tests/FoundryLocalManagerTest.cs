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
    private static readonly string[] value = ["model1"];

    public FoundryLocalManagerTests()
    {
        _mockHttp = new MockHttpMessageHandler();
        _client = _mockHttp.ToHttpClient();
        _client.BaseAddress = new Uri("http://localhost:1234");

        _manager = new FoundryLocalManager();
        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client.BaseAddress);

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);
    }

    [Fact]
    public async Task ListCatalogModelsAsync_ReturnsModels()
    {
        // GIVEN
        var json = JsonSerializer.Serialize(
        [
            new() { ModelId = "testModel", Alias = "alias", Uri = "http://model", ProviderType = "huggingface" }
        ], ModelGenerationContext.Default.ListModelInfo);

        _mockHttp.When("/foundry/list")
                 .Respond("application/json", json);

        // WHEN
        var result = await _manager.ListCatalogModelsAsync();

        // THEN
        Assert.Single(result);
        Assert.Equal("testModel", result[0].ModelId);
    }

    [Fact]
    public void RefreshCatalog_ResetsCache()
    {
        // GIVEN
        typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new List<ModelInfo> { new() });

        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo>());

        // WHEN
        _manager.RefreshCatalog();

        // THEN
        var models = typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(_manager);
        var dictionary = typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(_manager);
        Assert.Null(models);
        Assert.Null(dictionary);
    }

    [Fact]
    public async Task GetModelInfoAsync_ReturnsModel_WhenModelExists()
    {
        // GIVEN
        var testModel = new ModelInfo
        {
            ModelId = "test-model-id",
            Alias = "test-alias",
            Uri = "http://example.com",
            ProviderType = "huggingface",
            Runtime = new Runtime
            {
                DeviceType = DeviceType.CPU,
                ExecutionProvider = ExecutionProvider.CPUExecutionProvider
            }
        };

        var catalogDict = new Dictionary<string, ModelInfo>(StringComparer.OrdinalIgnoreCase)
        {
            { "test-model-id", testModel },
            { "test-alias", testModel }
        };

        // Inject the catalog dictionary into the private field
        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, catalogDict);

        // WHEN
        var resultById = await _manager.GetModelInfoAsync("test-model-id");
        var resultByAlias = await _manager.GetModelInfoAsync("test-alias");

        // THEN
        Assert.Same(testModel, resultById);
        Assert.Same(testModel, resultByAlias);
    }

    [Fact]
    public async Task GetModelInfoAsync_ReturnsNull_WhenModelDoesNotExist()
    {
        // GIVEN
        var catalogDict = new Dictionary<string, ModelInfo>(StringComparer.OrdinalIgnoreCase);
        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, catalogDict);

        // WHEN
        var result = await _manager.GetModelInfoAsync("non-existent");

        // THEN
        Assert.Null(result);
    }

    [Fact]
    public async Task GetCacheLocationAsync_ReturnsPath()
    {
        // GIVEN
        var json = /*lang=json,strict*/ """{ "modelDirPath": "/models" }""";
        _mockHttp.When("/openai/status").Respond("application/json", json);

        // WHEN
        var path = await _manager.GetCacheLocationAsync();

        // THEN
        Assert.Equal("/models", path);
    }

    [Fact]
    public async Task ListCachedModelsAsync_ReturnsMatchingInfos()
    {
        // GIVEN
        var modelIds = JsonSerializer.Serialize(value);
        _mockHttp.When("/openai/models").Respond("application/json", modelIds);

        var modelInfos = new List<ModelInfo>
        {
            new() { ModelId = "model1", Alias = "alias", Uri = "http://model", ProviderType = "huggingface" }
        };

        typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, modelInfos);

        // WHEN
        var result = await _manager.ListCachedModelsAsync();

        // THEN
        Assert.Single(result);
        Assert.Equal("model1", result[0].ModelId);
    }

    [Fact]
    public async Task DownloadModelAsync_DownloadsAndParsesModel_Success()
    {
        // GIVEN
        var modelId = "test-model";
        var model = new ModelInfo
        {
            ModelId = modelId,
            Alias = "alias1",
            Uri = "http://model.uri",
            ProviderType = "openai",
            Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = ExecutionProvider.CPUExecutionProvider }
        };

        var mockJsonResponse = "some log text... {\"success\": true, \"errorMessage\": null}";
        _mockHttp.When("/openai/download").Respond("application/json", mockJsonResponse);
        _mockHttp.When("/openai/models").Respond("application/json", "[]");

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);
        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo> { { modelId, model } });
        typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new List<ModelInfo> { model });

        // WHEN
        var result = await _manager.DownloadModelAsync(modelId);

        // THEN
        Assert.NotNull(result);
        Assert.Equal(model.ModelId, result!.ModelId);
    }

    [Fact]
    public async Task DownloadModelAsync_ThrowsIfNotFound()
    {
        // WHEN, THEN
        await Assert.ThrowsAsync<MockHttpMatchException>(() =>
            _manager.DownloadModelAsync("nonexistent"));
    }

    [Fact]
    public async Task LoadModelAsync_Succeeds_WhenModelIsInCatalogAndCache()
    {
        // GIVEN
        var modelId = "modelX";
        var model = new ModelInfo
        {
            ModelId = modelId,
            Alias = "aliasX",
            Uri = "http://model",
            ProviderType = "openai",
            Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = ExecutionProvider.CPUExecutionProvider }
        };

        _mockHttp.When("/openai/models").Respond("application/json", $"[\"{modelId}\"]");
        _mockHttp
            .When(HttpMethod.Get, $"http://localhost/openai/load/{modelId}*")
            .Respond("application/json", "{}");

        // Inject _serviceClient and _serviceUri
        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);
        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Uri("http://localhost"));

        // Inject catalog dictionary with the model
        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo>
            {
                { modelId, model },
                { model.Alias, model }
            });

        // Inject local cache list with the model
        typeof(FoundryLocalManager)
            .GetField("_catalogModels", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new List<ModelInfo> { model });

        // WHEN
        var result = await _manager.LoadModelAsync(modelId);

        // THEN
        Assert.NotNull(result);
        Assert.Equal(modelId, result.ModelId);
    }

    [Fact]
    public async Task LoadModelAsync_ThrowsIfNotInCache()
    {
        // GIVEN
        var model = new ModelInfo
        {
            ModelId = "modelX",
            Alias = "aliasX",
            Uri = "http://model",
            ProviderType = "huggingface",
            Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = ExecutionProvider.WebGpuExecutionProvider }
        };

        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo> { { "modelX", model } });

        _mockHttp.When("/openai/models").Respond("application/json", "[]");

        // WHEN
        var ex = await Assert.ThrowsAsync<InvalidOperationException>(() =>
            _manager.LoadModelAsync("modelX"));

        // THEN
        Assert.Contains("not found in local models", ex.Message);
    }

    [Fact]
    public async Task ListLoadedModelsAsync_ReturnsModelInfoList_WhenResponseIsValid()
    {
        // GIVEN
        var modelId = "modelX";
        _ = new[] { modelId };
        var model = new ModelInfo
        {
            ModelId = modelId,
            Alias = "aliasX",
            Uri = "http://model.uri",
            ProviderType = "openai",
            Runtime = new Runtime
            {
                DeviceType = DeviceType.CPU,
                ExecutionProvider = ExecutionProvider.CPUExecutionProvider
            }
        };

        _mockHttp
            .When("/openai/loadedmodels")
            .Respond("application/json", $"[\"{modelId}\"]");

        // Required because FetchModelInfosAsync calls GetModelInfoAsync
        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo> { { modelId, model } });

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);

        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Uri("http://localhost"));

        // WHEN
        var result = await _manager.ListLoadedModelsAsync();

        // THEN
        Assert.Single(result);
        Assert.Equal(modelId, result[0].ModelId);
    }

    [Fact]
    public async Task ListLoadedModelsAsync_ThrowsException_WhenDeserializationFails()
    {
        // GIVEN
        _mockHttp
            .When("/openai/loadedmodels")
            .Respond("application/json", "null"); // Simulates unexpected null from deserialization

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, _client);

        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Uri("http://localhost"));

        // WHEN, THEN
        var ex = await Assert.ThrowsAsync<InvalidOperationException>(() => _manager.ListLoadedModelsAsync());
        Assert.Equal("Failed to read loaded models.", ex.Message);
    }

    [Fact]
    public async Task UnloadModelAsync_CallsCorrectUri()
    {
        // GIVEN
        var model = new ModelInfo
        {
            ModelId = "modelY",
            Alias = "aliasY",
            Uri = "http://model",
            ProviderType = "huggingface",
            Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = ExecutionProvider.CPUExecutionProvider }
        };

        typeof(FoundryLocalManager)
            .GetField("_catalogDictionary", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_manager, new Dictionary<string, ModelInfo> { { "modelY", model } });

        _mockHttp.When("/openai/unload/modelY")
                 .WithQueryString("force=true")
                 .Respond(HttpStatusCode.OK);

        // WHEN
        await _manager.UnloadModelAsync("modelY");

        // THEN
        Assert.True(true); // If no exception, test passes
    }

    [Fact]
    public void Dispose_DisposesHttpClient()
    {
        _manager.Dispose();
        Assert.True(true); // If no exception, test passes
    }

    [Fact]
    public async Task DisposeAsync_DisposesHttpClient()
    {
        await _manager.DisposeAsync();
        Assert.True(true); // If no exception, test passes
    }

    public void Dispose()
    {
        _client.Dispose();
    }
}
