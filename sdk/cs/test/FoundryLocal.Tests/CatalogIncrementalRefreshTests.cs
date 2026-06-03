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

/// <summary>
/// Catalog.UpdateModels incremental-refresh behavior.
///
/// The refresh path is shared by all Catalog methods. These tests pin down
/// the contract that externally-held IModel references and per-Model variant
/// selection survive across forced refreshes when the underlying model id is
/// still present in the fresh catalog. They guard against regressing back to
/// the clear-and-rebuild pattern that churned wrapper identity on every
/// refresh.
/// </summary>
internal sealed class CatalogIncrementalRefreshTests
{
    private static ModelInfo MakeModelInfo(string id, string alias, bool cached, long maxOutputTokens = 1024L)
    {
        var version = int.Parse(id.Split(':').Last());
        return new ModelInfo
        {
            Id = id,
            Name = alias,
            Version = version,
            Alias = alias,
            DisplayName = alias,
            ProviderType = "local",
            Uri = $"local://{alias}/{version}",
            ModelType = "ONNX",
            Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
            Cached = cached,
            MaxOutputTokens = maxOutputTokens,
        };
    }

    /// <summary>
    /// Builds a Catalog whose <c>get_model_list</c> IPC returns
    /// <paramref name="responses"/>[N] on the (N+1)-th call (clamping to the
    /// last entry). <c>get_cached_models</c> and the load manager's loaded
    /// list both return empty so only the explicit refresh path runs.
    /// Force-refreshes are triggered via <see cref="Catalog.InvalidateCache"/>
    /// followed by a public call (mirrors what <c>UpdateModels(force: true)</c>
    /// does: bypasses the TTL gate, then re-runs the rebuild).
    /// </summary>
    private static async Task<Catalog> CreateCatalogAsync(IReadOnlyList<IReadOnlyList<ModelInfo>> responses)
    {
        var call = 0;
        var mockCoreInterop = new Mock<ICoreInterop>();

        mockCoreInterop.Setup(x => x.ExecuteCommand("get_catalog_name", It.IsAny<CoreInteropRequest?>()))
            .Returns(new ICoreInterop.Response { Data = "TestCatalog", Error = null });

        mockCoreInterop.Setup(x => x.ExecuteCommandAsync("get_model_list", It.IsAny<CoreInteropRequest?>(),
                                                         It.IsAny<CancellationToken?>()))
            .ReturnsAsync(() =>
            {
                var idx = System.Math.Min(call, responses.Count - 1);
                call++;
                var data = JsonSerializer.Serialize(
                    responses[idx].ToList(), JsonSerializationContext.Default.ListModelInfo);
                return new ICoreInterop.Response { Data = data, Error = null };
            });

        mockCoreInterop.Setup(x => x.ExecuteCommandAsync("get_cached_models", It.IsAny<CoreInteropRequest?>(),
                                                         It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Data = "[]", Error = null });

        var mockLoadManager = new Mock<IModelLoadManager>();
        mockLoadManager.Setup(x => x.ListLoadedModelsAsync(It.IsAny<CancellationToken?>()))
            .ReturnsAsync(System.Array.Empty<string>());

        return await Catalog.CreateAsync(mockLoadManager.Object, mockCoreInterop.Object,
                                         NullLogger<Catalog>.Instance, null);
    }

    private static async Task ForceRefreshAsync(Catalog catalog)
    {
        catalog.InvalidateCache();
        // Any public method that calls UpdateModels works; ListModelsAsync is the cheapest.
        await catalog.ListModelsAsync();
    }

    [Test]
    public async Task PreservesIModelIdentityAcrossForcedRefresh()
    {
        // A held IModel reference must survive a forced refresh when the id is
        // still present, so user code holding the wrapper keeps working and
        // identity (ReferenceEquals) comparisons remain meaningful.
        var infos = new List<ModelInfo> { MakeModelInfo("a:1", "alpha", cached: true) };
        var catalog = await CreateCatalogAsync(new[] { infos, infos });

        var first = await catalog.GetModelAsync("alpha");
        var firstVariant = await catalog.GetModelVariantAsync("a:1");
        await ForceRefreshAsync(catalog);
        var second = await catalog.GetModelAsync("alpha");
        var secondVariant = await catalog.GetModelVariantAsync("a:1");

        await Assert.That(ReferenceEquals(first, second)).IsTrue();
        await Assert.That(ReferenceEquals(firstVariant, secondVariant)).IsTrue();
    }

    [Test]
    public async Task PreservesExplicitVariantSelectionAcrossRefresh()
    {
        // SelectVariant() made by the user must survive a forced refresh so
        // subsequent operations (load, unload, etc.) still target the variant
        // the user picked.
        var v1 = MakeModelInfo("multi:1", "multi", cached: true);
        var v2 = MakeModelInfo("multi:2", "multi", cached: true);
        var infos = new List<ModelInfo> { v1, v2 };
        var catalog = await CreateCatalogAsync(new[] { infos, infos });

        var model = (Model)(await catalog.GetModelAsync("multi"))!;
        var variantV2 = model.Variants.First(v => v.Id == "multi:2");
        model.SelectVariant(variantV2);
        await Assert.That(model.Id).IsEqualTo("multi:2");

        await ForceRefreshAsync(catalog);
        await Assert.That(model.Id).IsEqualTo("multi:2");
    }

    [Test]
    public async Task RefreshesModelInfoOnExistingVariant()
    {
        // When Cached (or any ModelInfo field) flips for a known id, the
        // already-held ModelVariant must surface the fresh value. Incremental
        // refresh updates the wrapper's Info snapshot in place rather than
        // replacing the wrapper.
        var firstState = new List<ModelInfo>
        {
            MakeModelInfo("a:1", "alpha", cached: false, maxOutputTokens: 1024L),
        };
        var secondState = new List<ModelInfo>
        {
            MakeModelInfo("a:1", "alpha", cached: true, maxOutputTokens: 2048L),
        };
        var catalog = await CreateCatalogAsync(new[] { firstState, secondState });

        var variant = (await catalog.GetModelVariantAsync("a:1"))!;
        await Assert.That(variant.Info.Cached).IsFalse();
        await Assert.That(variant.Info.MaxOutputTokens).IsEqualTo(1024L);

        await ForceRefreshAsync(catalog);
        await Assert.That(variant.Info.Cached).IsTrue();
        await Assert.That(variant.Info.MaxOutputTokens).IsEqualTo(2048L);
    }

    [Test]
    public async Task DropsStaleIdsOnRefresh()
    {
        // Ids no longer present in the fresh catalog must be evicted so
        // GetModelAsync / GetModelVariantAsync no longer resolve them.
        var firstState = new List<ModelInfo>
        {
            MakeModelInfo("a:1", "alpha", cached: true),
            MakeModelInfo("b:1", "beta",  cached: true),
        };
        var secondState = new List<ModelInfo> { MakeModelInfo("a:1", "alpha", cached: true) };
        var catalog = await CreateCatalogAsync(new[] { firstState, secondState });

        await Assert.That(await catalog.GetModelVariantAsync("b:1")).IsNotNull();
        await ForceRefreshAsync(catalog);
        await Assert.That(await catalog.GetModelVariantAsync("b:1")).IsNull();
        await Assert.That(await catalog.GetModelAsync("beta")).IsNull();
    }

    [Test]
    public async Task AddsNewIdsOnRefresh()
    {
        // New ids appearing on a refresh (e.g. BYOM stubs added since last warm)
        // must be inserted as fresh wrappers.
        var firstState = new List<ModelInfo> { MakeModelInfo("a:1", "alpha", cached: true) };
        var secondState = new List<ModelInfo>
        {
            MakeModelInfo("a:1", "alpha", cached: true),
            MakeModelInfo("byom:1", "byom-new", cached: true),
        };
        var catalog = await CreateCatalogAsync(new[] { firstState, secondState });

        // Warm the initial state and confirm the new id is absent.
        var initialModels = await catalog.ListModelsAsync();
        await Assert.That(initialModels.Any(m => m.Alias == "byom-new")).IsFalse();

        await ForceRefreshAsync(catalog);
        var newVariant = await catalog.GetModelVariantAsync("byom:1");
        var newModel = await catalog.GetModelAsync("byom-new");
        await Assert.That(newVariant).IsNotNull();
        await Assert.That(newModel).IsNotNull();
        await Assert.That(newModel!.Id).IsEqualTo("byom:1");
    }

    [Test]
    public async Task FallsBackToFirstCachedWhenSelectedVariantRemoved()
    {
        // If the user's selected variant disappears on a refresh (rare — Core
        // dropped it from the catalog), the Model wrapper must fall back to a
        // sensible default so subsequent ops do not target a stale id.
        var v1 = MakeModelInfo("multi:1", "multi", cached: true);
        var v2 = MakeModelInfo("multi:2", "multi", cached: true);
        var firstState = new List<ModelInfo> { v1, v2 };
        var secondState = new List<ModelInfo> { v1 };
        var catalog = await CreateCatalogAsync(new[] { firstState, secondState });

        var model = (Model)(await catalog.GetModelAsync("multi"))!;
        model.SelectVariant(model.Variants.First(v => v.Id == "multi:2"));
        await Assert.That(model.Id).IsEqualTo("multi:2");

        await ForceRefreshAsync(catalog);
        await Assert.That(model.Id).IsEqualTo("multi:1");
    }
}
