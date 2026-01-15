// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

public class FoundryLocalManagerTests
{
    [Test]
    public async Task Manager_GetCatalog_Succeeds()
    {
        var catalog = await FoundryLocalManager.Instance.GetCatalogAsync() as Catalog;
        await Assert.That(catalog).IsNotNull();
        await Assert.That(catalog!.Name).IsNotNullOrWhitespace();

        var models = await catalog.ListModelsAsync();
        await Assert.That(models).IsNotNull().And.IsNotEmpty();

        foreach (var model in models)
        {
            Console.WriteLine($"Model Alias: {model.Alias}, Variants: {model.Variants.Count}");
            Console.WriteLine($"Selected Variant Id: {model.SelectedVariant?.Id ?? "none"}");

            // variants should be in sorted order

            DeviceType lastDeviceType = DeviceType.Invalid;
            var lastName = string.Empty;
            var lastVersion = int.MaxValue;

            foreach (var variant in model.Variants)
            {
                Console.WriteLine($"  Id: {variant.Id}, Cached={variant.Info.Cached}");

                // variants are grouped by name
                // check if variants are sorted by device type and version
                if ((variant.Info.Name == lastName) &&
                    ((variant.Info.Runtime?.DeviceType > lastDeviceType) ||
                     (variant.Info.Runtime?.DeviceType == lastDeviceType && variant.Info.Version > lastVersion)))
                {
                    Assert.Fail($"Variant {variant.Id} is not in the expected order.");
                }

                lastDeviceType = variant.Info.Runtime?.DeviceType ?? DeviceType.Invalid;
                lastName = variant.Info.Name;
                lastVersion = variant.Info.Version;
            }
        }
    }

    [Test]
    public async Task Catalog_ListCachedLoadUnload_Succeeds()
    {
        List<string> logSink = new();
        var logger = Utils.CreateCapturingLoggerMock(logSink);
        using var loadManager = new ModelLoadManager(null, Utils.CoreInterop, logger.Object);

        List<Utils.InteropCommandInterceptInfo> intercepts = new()
        {
            new Utils.InteropCommandInterceptInfo
            {
                CommandName = "initialize",
                CommandInput = null,
                ResponseData = "Success",
                ResponseError = null
            }
        };
        var coreInterop = Utils.CreateCoreInteropWithIntercept(Utils.CoreInterop, intercepts);
        using var catalog = await Catalog.CreateAsync(loadManager, coreInterop.Object, logger.Object);
        await Assert.That(catalog).IsNotNull();

        var models = await catalog.ListModelsAsync();
        await Assert.That(models).IsNotNull().And.IsNotEmpty();

        var cachedModels = await catalog.GetCachedModelsAsync();
        await Assert.That(cachedModels).IsNotNull();

        if (cachedModels.Count == 0)
        {
            Console.WriteLine("No cached models found; skipping get path/load/unload test.");
            return;
        }

        // find smallest. pick first if no local models have size info.
        var smallest = cachedModels.Where(m => m.Info.FileSizeMb > 0).OrderBy(m => m.Info.FileSizeMb).FirstOrDefault();
        var variant = smallest ?? cachedModels[0];

        Console.WriteLine($"Testing GetPath/Load/Unload with ModelId: {variant.Id}");
        var path = await variant.GetPathAsync();
        Console.WriteLine($"Model path: {path}");
        await variant.LoadAsync();

        // We unload any loaded models during cleanup for all tests
        // await variant.UnloadAsync();
    }
}

