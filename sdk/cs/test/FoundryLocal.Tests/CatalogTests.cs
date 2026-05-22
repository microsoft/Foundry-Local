// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging.Abstractions;

using Moq;

internal sealed class CatalogTests
{
    [Test]
    public async Task GetLatestVersion_Works()
    {
        // Create test data with 3 entries for a model with different versions
        // Sorted by version (descending), so version 3 is first (latest)
        var testModelInfos = new List<ModelInfo>
        {
            new()
            {
                Id = "test-model:3",
                Name = "test-model",
                Version = 3,
                Alias = "test-alias",
                DisplayName = "Test Model",
                ProviderType = "test",
                Uri = "test://model/3",
                ModelType = "ONNX",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Cached = false
            },
            new()
            {
                Id = "test-model:2",
                Name = "test-model",
                Version = 2,
                Alias = "test-alias",
                DisplayName = "Test Model",
                ProviderType = "test",
                Uri = "test://model/2",
                ModelType = "ONNX",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Cached = false
            },
            new()
            {
                Id = "test-model:1",
                Name = "test-model",
                Version = 1,
                Alias = "test-alias",
                DisplayName = "Test Model",
                ProviderType = "test",
                Uri = "test://model/1",
                ModelType = "ONNX",
                Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                Cached = false
            }
        };

        // Serialize the test data
        var modelListJson = JsonSerializer.Serialize(testModelInfos, JsonSerializationContext.Default.ListModelInfo);

        // Create mock ICoreInterop
        var mockCoreInterop = new Mock<ICoreInterop>();

        // Mock get_catalog_name
        mockCoreInterop.Setup(x => x.ExecuteCommand("get_catalog_name", It.IsAny<CoreInteropRequest?>()))
            .Returns(new ICoreInterop.Response { Data = "TestCatalog", Error = null });

        // Mock get_model_list
        mockCoreInterop.Setup(x => x.ExecuteCommandAsync("get_model_list", It.IsAny<CoreInteropRequest?>(), It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Data = modelListJson, Error = null });

        // Create mock IModelLoadManager
        var mockLoadManager = new Mock<IModelLoadManager>();

        // Create Catalog instance directly (internals are visible to test project)
        var catalog = await Catalog.CreateAsync(mockLoadManager.Object, mockCoreInterop.Object,
                                                NullLogger<Catalog>.Instance, null);

        // Get the model
        var model = await catalog.GetModelAsync("test-alias");
        await Assert.That(model).IsNotNull();

        // Verify we have 3 variants
        await Assert.That(model!.Variants).HasCount().EqualTo(3);

        // Get the variants - they should be sorted by version (descending)
        var variants = model.Variants.ToList();
        var latestVariant = variants[0]; // version 3
        var middleVariant = variants[1]; // version 2
        var oldestVariant = variants[2]; // version 1

        await Assert.That(latestVariant.Id).IsEqualTo("test-model:3");
        await Assert.That(middleVariant.Id).IsEqualTo("test-model:2");
        await Assert.That(oldestVariant.Id).IsEqualTo("test-model:1");

        // Test GetLatestVersionAsync with all 3 variants - should always return the first (version 3)
        var result1 = await catalog.GetLatestVersionAsync(latestVariant);
        await Assert.That(result1.Id).IsEqualTo("test-model:3");

        var result2 = await catalog.GetLatestVersionAsync(middleVariant);
        await Assert.That(result2.Id).IsEqualTo("test-model:3");

        var result3 = await catalog.GetLatestVersionAsync(oldestVariant);
        await Assert.That(result3.Id).IsEqualTo("test-model:3");

        // Test with Model input - when latest is selected, should get Model not ModelVariant back 
        model.SelectVariant(latestVariant);
        var result4 = await catalog.GetLatestVersionAsync(model);
        await Assert.That(result4).IsEqualTo(model);
    }
}
