// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text.Json;
using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;
using Moq;

/// <summary>
/// Unit tests for HuggingFaceCatalog — validates GetModelAsync lookup by alias and URI,
/// and that DownloadModelAsync accepts model URI from registration.
/// </summary>
public class HuggingFaceCatalogTests
{
    private static ModelInfo CreateTestModelInfo(string alias, string id, string uri)
    {
        return new ModelInfo
        {
            Id = id,
            Name = alias,
            DisplayName = alias,
            Alias = alias,
            Uri = uri,
            ProviderType = "HuggingFace",
            Version = 0,
            ModelType = "ONNX",
            Publisher = "test-org",
            Task = "chat-completion",
            License = "MIT",
            FileSizeMb = 100
        };
    }

    private static (Mock<ICoreInterop> coreInterop, Mock<IModelLoadManager> loadManager, Mock<ILogger> logger)
        CreateMocks(ModelInfo modelInfo)
    {
        var logger = Utils.CreateCapturingLoggerMock([]);
        var loadManager = new Mock<IModelLoadManager>();
        loadManager.Setup(x => x.ListLoadedModelsAsync(It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(Array.Empty<string>());

        var coreInterop = new Mock<ICoreInterop>();

        // Mock register_model to return the ModelInfo JSON
        var modelInfoJson = JsonSerializer.Serialize(modelInfo, JsonSerializationContext.Default.ModelInfo);
        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "register_model"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response { Data = modelInfoJson, Error = null });

        // Mock get_cached_models to return empty
        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "get_cached_models"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response { Data = "[]", Error = null });

        return (coreInterop, loadManager, logger);
    }

    [Test]
    public async Task GetModelAsync_ByAlias_ReturnsRegisteredModel()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        // Register the model
        var registered = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");
        await Assert.That(registered.Alias).IsEqualTo("phi-3-mini-4k");

        // Lookup by alias
        var found = await catalog.GetModelAsync("phi-3-mini-4k");
        await Assert.That(found).IsNotNull();
        await Assert.That(found!.Alias).IsEqualTo("phi-3-mini-4k");
    }

    [Test]
    public async Task GetModelAsync_ByUri_ReturnsRegisteredModel()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        var registered = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        // Lookup by full URI (what SelectedVariant.Info.Uri returns)
        var found = await catalog.GetModelAsync("https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");
        await Assert.That(found).IsNotNull();
        await Assert.That(found!.Alias).IsEqualTo("phi-3-mini-4k");
    }

    [Test]
    public async Task GetModelAsync_ByOrgRepo_ReturnsRegisteredModel()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        // Lookup by org/repo identifier
        var found = await catalog.GetModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");
        await Assert.That(found).IsNotNull();
        await Assert.That(found!.Alias).IsEqualTo("phi-3-mini-4k");
    }

    [Test]
    public async Task GetModelAsync_NotRegistered_ReturnsNull()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        // Don't register anything — lookup should return null
        var found = await catalog.GetModelAsync("nonexistent-model");
        await Assert.That(found).IsNull();
    }

    [Test]
    public async Task GetModelAsync_BySubpathUri_ReturnsRegisteredModel()
    {
        var modelInfo = CreateTestModelInfo(
            "gemma-3-4b-it",
            "onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile:abcd1234",
            "https://huggingface.co/onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        await catalog.RegisterModelAsync("onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        // Lookup by the full URI with subpath
        var found = await catalog.GetModelAsync(
            "https://huggingface.co/onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");
        await Assert.That(found).IsNotNull();
        await Assert.That(found!.Alias).IsEqualTo("gemma-3-4b-it");
    }

    [Test]
    public async Task DownloadModelAsync_WithModelUri_SucceedsWhenModelRegistered()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        // Mock download_model to return the org/repo identifier (as Core does)
        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "download_model"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response
                   {
                       Data = "microsoft/Phi-3-mini-4k-instruct-onnx",
                       Error = null
                   });

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        // Register first
        var registered = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        // Download using model's URI (the simplified API pattern)
        var uri = registered.SelectedVariant.Info.Uri;
        var downloaded = await catalog.DownloadModelAsync(uri);

        await Assert.That(downloaded).IsNotNull();
        await Assert.That(downloaded.Alias).IsEqualTo("phi-3-mini-4k");
    }

    [Test]
    public async Task DownloadModelAsync_WithSubpathUri_SucceedsWhenModelRegistered()
    {
        var modelInfo = CreateTestModelInfo(
            "gemma-3-4b-it",
            "onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile:abcd1234",
            "https://huggingface.co/onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "download_model"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response
                   {
                       Data = "onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile",
                       Error = null
                   });

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        var registered = await catalog.RegisterModelAsync(
            "onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        // Download using model's URI (subpath model)
        var uri = registered.SelectedVariant.Info.Uri;
        var downloaded = await catalog.DownloadModelAsync(uri);

        await Assert.That(downloaded).IsNotNull();
        await Assert.That(downloaded.Alias).IsEqualTo("gemma-3-4b-it");
    }

    [Test]
    public async Task RegisterModelAsync_Idempotent_ReturnsSameAlias()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        var first = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");
        var second = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        await Assert.That(first.Alias).IsEqualTo(second.Alias);
    }

    [Test]
    public async Task SelectedVariantInfoUri_ReturnsExpectedValue()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        var model = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        // SelectedVariant.Info.Uri holds the HuggingFace URL
        await Assert.That(model.SelectedVariant.Info.Uri)
            .IsEqualTo("https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");
    }

    [Test]
    public async Task DownloadModelAsync_UsingSelectedVariantInfoUri_SucceedsWithoutRepeatingIdentifier()
    {
        var modelInfo = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");

        var (coreInterop, loadManager, logger) = CreateMocks(modelInfo);

        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "download_model"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response
                   {
                       Data = "microsoft/Phi-3-mini-4k-instruct-onnx",
                       Error = null
                   });

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        var registered = await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");

        // Simplified pattern: use SelectedVariant.Info.Uri instead of repeating the raw string
        var downloaded = await catalog.DownloadModelAsync(registered.SelectedVariant.Info.Uri);

        await Assert.That(downloaded).IsNotNull();
        await Assert.That(downloaded.Alias).IsEqualTo("phi-3-mini-4k");
        await Assert.That(downloaded.SelectedVariant.Info.Uri).IsEqualTo(registered.SelectedVariant.Info.Uri);
    }

    [Test]
    public async Task ListModelsAsync_ReturnsAllRegisteredModelsSortedByAlias()
    {
        // Register two models — need separate mock setup for sequential calls
        var phi = CreateTestModelInfo(
            "phi-3-mini-4k",
            "microsoft/Phi-3-mini-4k-instruct-onnx:abcd1234",
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");
        var gemma = CreateTestModelInfo(
            "gemma-3-4b-it",
            "onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile:efgh5678",
            "https://huggingface.co/onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        var logger = Utils.CreateCapturingLoggerMock([]);
        var loadManager = new Mock<IModelLoadManager>();
        loadManager.Setup(x => x.ListLoadedModelsAsync(It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(Array.Empty<string>());
        var coreInterop = new Mock<ICoreInterop>();

        // Return phi on first call, gemma on second
        var callIndex = 0;
        var models = new[] { phi, gemma };
        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "register_model"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(() =>
                   {
                       var json = JsonSerializer.Serialize(models[callIndex++],
                           JsonSerializationContext.Default.ModelInfo);
                       return new ICoreInterop.Response { Data = json, Error = null };
                   });

        coreInterop.Setup(x => x.ExecuteCommandAsync(
                       It.Is<string>(s => s == "get_cached_models"),
                       It.IsAny<CoreInteropRequest?>(),
                       It.IsAny<CancellationToken?>()))
                   .ReturnsAsync(new ICoreInterop.Response { Data = "[]", Error = null });

        using var catalog = await HuggingFaceCatalog.CreateAsync(
            loadManager.Object, coreInterop.Object, logger.Object);

        await catalog.RegisterModelAsync("microsoft/Phi-3-mini-4k-instruct-onnx");
        await catalog.RegisterModelAsync("onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");

        var list = await catalog.ListModelsAsync();

        await Assert.That(list.Count).IsEqualTo(2);
        // Sorted alphabetically by alias
        await Assert.That(list[0].Alias).IsEqualTo("gemma-3-4b-it");
        await Assert.That(list[1].Alias).IsEqualTo("phi-3-mini-4k");
        // Each model has correct URI via SelectedVariant.Info.Uri
        await Assert.That(list[0].SelectedVariant.Info.Uri)
            .IsEqualTo("https://huggingface.co/onnxruntime/Gemma-3-ONNX/gemma-3-4b-it/cpu_and_mobile");
        await Assert.That(list[1].SelectedVariant.Info.Uri)
            .IsEqualTo("https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx");
    }
}
