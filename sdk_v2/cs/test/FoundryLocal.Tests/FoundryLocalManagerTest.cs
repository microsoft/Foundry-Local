// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Linq;

using Microsoft.AI.Foundry.Local;
using TUnit.Core.Exceptions;

[SkipUnlessIntegration]
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
            Console.WriteLine($"Selected Variant Id: {model.Id ?? "none"}");

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
        var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();
        await Assert.That(catalog).IsNotNull();

        var models = await catalog.ListModelsAsync();
        await Assert.That(models).IsNotNull().And.IsNotEmpty();

        var cachedModels = await catalog.GetCachedModelsAsync();
        await Assert.That(cachedModels).IsNotNull();

        if (cachedModels.Count == 0)
        {
            throw new SkipTestException("No cached models found; skipping get path/load/unload test.");
        }

        // find smallest. pick first if no local models have size info.
        // Restrict to CPUExecutionProvider — other EPs (CUDA, WebGPU, ...) require a
        // bootstrapper to be downloaded/registered first via DownloadAndRegisterEps.
        var loadable = cachedModels.Where(m => m.Info.Runtime?.ExecutionProvider == "CPUExecutionProvider").ToList();
        if (loadable.Count == 0)
        {
            throw new SkipTestException("No cached CPU-EP models; skipping load/unload test.");
        }

        var smallest = loadable.Where(m => m.Info.FileSizeMb > 0).OrderBy(m => m.Info.FileSizeMb).FirstOrDefault();
        var variant = smallest ?? loadable[0];

        Console.WriteLine($"Testing GetPath/Load/Unload with ModelId: {variant.Id}");
        var path = await variant.GetPathAsync();
        Console.WriteLine($"Model path: {path}");
        await variant.LoadAsync();

        // We unload any loaded models during cleanup for all tests
        // await variant.UnloadAsync();
    }
}

