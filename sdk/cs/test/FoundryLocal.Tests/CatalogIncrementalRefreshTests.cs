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
/// <see cref="Catalog.UpdateModels"/> incremental-refresh behavior.
///
/// The refresh path is shared by all Catalog methods. These tests pin down the
/// contract that externally-held <see cref="IModel"/> references and per-Model
/// variant selection survive across forced refreshes when the underlying model
/// id is still present in the fresh catalog.
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
                var idx = Math.Min(call, responses.Count - 1);
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
            .ReturnsAsync([]);

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
    public async Task PreservesIdentityAndPropagatesInfoOnRefresh()
    {
        // A held Model / ModelVariant reference must survive a forced refresh
        // when the id is still present (identity preserved) AND surface fresh
        // ModelInfo through the same reference. Mutating Info is the natural
        // way to prove that the post-refresh wrapper IS the pre-refresh
        // wrapper, not a fresh one that happens to compare equal.
        List<ModelInfo> firstState =
        [
            MakeModelInfo("a:1", "alpha", cached: false, maxOutputTokens: 1024L),
        ];
        List<ModelInfo> secondState =
        [
            MakeModelInfo("a:1", "alpha", cached: true, maxOutputTokens: 2048L),
        ];
        var catalog = await CreateCatalogAsync([firstState, secondState]);

        var firstModel = await catalog.GetModelAsync("alpha");
        var firstVariant = await catalog.GetModelVariantAsync("a:1");
        await Assert.That(firstVariant!.Info.Cached).IsFalse();
        await Assert.That(firstVariant.Info.MaxOutputTokens).IsEqualTo(1024L);

        await ForceRefreshAsync(catalog);

        var secondModel = await catalog.GetModelAsync("alpha");
        var secondVariant = await catalog.GetModelVariantAsync("a:1");
        await Assert.That(ReferenceEquals(firstModel, secondModel)).IsTrue();
        await Assert.That(ReferenceEquals(firstVariant, secondVariant)).IsTrue();
        // Same held reference now reflects the fresh ModelInfo.
        await Assert.That(firstVariant.Info.Cached).IsTrue();
        await Assert.That(firstVariant.Info.MaxOutputTokens).IsEqualTo(2048L);
    }

    [Test]
    public async Task SelectionSurvivesNormalRefreshAndFallsBackOnRemoval()
    {
        // Covers two inverse facets of RefreshVariants:
        //   1. When the selected variant's id is still present, SelectVariant()
        //      survives so subsequent ops (load, unload, ...) target the
        //      user's pick.
        //   2. When the selected variant is gone, the Model wrapper falls back
        //      to the first cached variant and the stale wrapper is evicted
        //      from Variants (so iterating Variants does not surface an id
        //      Core no longer knows about).
        var v1 = MakeModelInfo("multi:1", "multi", cached: true);
        var v2 = MakeModelInfo("multi:2", "multi", cached: true);
        List<ModelInfo> both = [v1, v2];
        List<ModelInfo> onlyV1 = [v1];
        var catalog = await CreateCatalogAsync([both, both, onlyV1]);

        var model = (Model)(await catalog.GetModelAsync("multi"))!;
        var variantV2 = model.Variants.First(v => v.Id == "multi:2");
        model.SelectVariant(variantV2);
        await Assert.That(model.Id).IsEqualTo("multi:2");

        // Phase 1: refresh with both variants — selection survives.
        await ForceRefreshAsync(catalog);
        await Assert.That(model.Id).IsEqualTo("multi:2");
        await Assert.That(model.Variants.Count).IsEqualTo(2);

        // Phase 2: refresh drops v2 — fall back to v1, evict v2 from Variants.
        await ForceRefreshAsync(catalog);
        await Assert.That(model.Id).IsEqualTo("multi:1");
        await Assert.That(model.Variants.Count).IsEqualTo(1);
        await Assert.That(model.Variants.Any(v => v.Id == "multi:2")).IsFalse();
        await Assert.That(model.Variants[0].Id).IsEqualTo("multi:1");
    }

    [Test]
    public async Task FallbackPrefersFirstCachedOverFirstVariant()
    {
        // Distinguishes the cached-fallback rung from the variants[0] rung in
        // RefreshVariants. Three variants where the first is uncached:
        // dropping the selected (cached) variant must fall back to the FIRST
        // CACHED variant (multi:2), not to variants[0] (multi:1, uncached).
        var v1Uncached = MakeModelInfo("multi:1", "multi", cached: false);
        var v2Cached = MakeModelInfo("multi:2", "multi", cached: true);
        var v3Cached = MakeModelInfo("multi:3", "multi", cached: true);
        List<ModelInfo> allThree = [v1Uncached, v2Cached, v3Cached];
        List<ModelInfo> withoutV3 = [v1Uncached, v2Cached];
        var catalog = await CreateCatalogAsync([allThree, withoutV3]);

        var model = (Model)(await catalog.GetModelAsync("multi"))!;
        var variantV3 = model.Variants.First(v => v.Id == "multi:3");
        model.SelectVariant(variantV3);
        await Assert.That(model.Id).IsEqualTo("multi:3");

        await ForceRefreshAsync(catalog);
        await Assert.That(model.Id).IsEqualTo("multi:2");
        await Assert.That(model.Variants.Count).IsEqualTo(2);
        await Assert.That(model.Variants.Any(v => v.Id == "multi:3")).IsFalse();
    }

    [Test]
    public async Task AppliesAddsAndRemovesOnRefresh()
    {
        // Ids no longer present must be evicted from both _modelIdToModelVariant
        // and _modelAliasToModel; new ids must be inserted as fresh wrappers.
        // Both directions are symmetric and share setup, so we cover them in
        // one test against a single refresh that does both at once.
        List<ModelInfo> firstState =
        [
            MakeModelInfo("a:1", "alpha", cached: true),
            MakeModelInfo("b:1", "beta",  cached: true),
        ];
        List<ModelInfo> secondState =
        [
            MakeModelInfo("a:1", "alpha", cached: true),
            MakeModelInfo("byom:1", "byom-new", cached: true),
        ];
        var catalog = await CreateCatalogAsync([firstState, secondState]);

        // Warm the catalog and confirm the pre-state. ListModelsAsync does not
        // trigger self-heal, so it is safe to probe before the forced refresh.
        var initialAliases = (await catalog.ListModelsAsync()).Select(m => m.Alias).ToHashSet();
        await Assert.That(initialAliases.Contains("alpha")).IsTrue();
        await Assert.That(initialAliases.Contains("beta")).IsTrue();
        await Assert.That(initialAliases.Contains("byom-new")).IsFalse();

        await ForceRefreshAsync(catalog);

        var newAliases = (await catalog.ListModelsAsync()).Select(m => m.Alias).ToHashSet();
        await Assert.That(newAliases.Contains("alpha")).IsTrue();
        await Assert.That(newAliases.Contains("beta")).IsFalse();
        await Assert.That(newAliases.Contains("byom-new")).IsTrue();

        // Variant-level eviction + insertion mirrors the alias-level change.
        // GetModelVariantAsync on a known id is a direct hit; on a gone id it
        // self-heals but the mock has already clamped to secondState, so the
        // self-heal IPC returns the same data and the assertion still holds.
        await Assert.That(await catalog.GetModelVariantAsync("byom:1")).IsNotNull();
        await Assert.That(await catalog.GetModelVariantAsync("b:1")).IsNull();
    }
}
