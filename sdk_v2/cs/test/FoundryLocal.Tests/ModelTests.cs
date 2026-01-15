// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;
using System.Collections.Generic;
using System.Threading.Tasks;

using Microsoft.Extensions.Logging.Abstractions;

using Moq;

internal sealed class ModelTests
{
    [Test]
    public async Task GetLastestVersion_Works()
    {
        var loadManager = new Mock<Detail.IModelLoadManager>();
        var coreInterop = new Mock<Detail.ICoreInterop>();
        var logger = NullLogger<ModelVariant>.Instance;

        var createModelInfo = (string name, int version) => new ModelInfo
        {
            Id = $"{name}:{version}",
            Alias = "model",
            Name = name,
            Version = version,
            Uri = "local://model",
            ProviderType = "local",
            ModelType = "test"
        };

        var variants = new List<ModelVariant>
        {
            new(createModelInfo("model_a", 4), loadManager.Object, coreInterop.Object, logger),
            new(createModelInfo("model_b", 3), loadManager.Object, coreInterop.Object, logger),
            new(createModelInfo("model_b", 2), loadManager.Object, coreInterop.Object, logger),
        };

        var model = new Model(variants[0], NullLogger<Model>.Instance);
        foreach (var variant in variants.Skip(1))
        {
            model.AddVariant(variant);
        }

        var latestA = model.GetLatestVersion(variants[0]);
        await Assert.That(latestA).IsEqualTo(variants[0]);

        var latestB = model.GetLatestVersion(variants[2]);
        await Assert.That(latestB).IsEqualTo(variants[1]);
    }
}
