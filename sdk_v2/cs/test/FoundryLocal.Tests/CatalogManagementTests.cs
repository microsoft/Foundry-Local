// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text.Json;
using Microsoft.AI.Foundry.Local.Detail;
using Moq;

public class CatalogManagementTests
{
    private static async Task<Catalog> CreateCatalogWithIntercepts(
        List<Utils.InteropCommandInterceptInfo> extra)
    {
        var logger = Utils.CreateCapturingLoggerMock([]);
        var lm = new Mock<IModelLoadManager>();
        lm.Setup(m => m.ListLoadedModelsAsync(It.IsAny<CancellationToken?>())).ReturnsAsync(Array.Empty<string>());

        List<Utils.InteropCommandInterceptInfo> intercepts =
        [
            new() { CommandName = "get_catalog_name", ResponseData = "Test" },
            new() { CommandName = "get_model_list",
                    ResponseData = JsonSerializer.Serialize(Utils.TestCatalog.TestCatalog,
                                                            JsonSerializationContext.Default.ListModelInfo) },
            new() { CommandName = "get_cached_model_ids", ResponseData = "[]" },
            .. extra
        ];

        var ci = Utils.CreateCoreInteropWithIntercept(Utils.CoreInterop, intercepts);
        return await Catalog.CreateAsync(lm.Object, ci.Object, logger.Object);
    }

    [Test]
    public async Task Test_AddAndSelectCatalog()
    {
        using var catalog = await CreateCatalogWithIntercepts(
        [
            new() { CommandName = "add_catalog", ResponseData = "OK" },
            new() { CommandName = "select_catalog", ResponseData = "OK" }
        ]);

        await catalog.AddCatalogAsync("priv", new Uri("https://mds.example.com"), "id", "secret");
        await catalog.SelectCatalogAsync("priv");
        await catalog.SelectCatalogAsync(null);
        await Assert.That(catalog).IsNotNull();
    }

    [Test]
    public async Task Test_GetCatalogNames()
    {
        using var catalog = await CreateCatalogWithIntercepts(
            [new() { CommandName = "get_catalog_names", ResponseData = "[\"public\",\"private\"]" }]);

        var names = await catalog.GetCatalogNamesAsync();
        await Assert.That(names.Count).IsEqualTo(2);
        await Assert.That(names).Contains("private");
    }
}
