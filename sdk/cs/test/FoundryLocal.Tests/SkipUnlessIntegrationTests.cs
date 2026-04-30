// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;

internal sealed class SkipUnlessIntegrationTests
{
    [Test]
    public async Task ShouldSkip_ReturnsTrue_WhenIntegrationTestsNotAvailable()
    {
        // IntegrationTestsAvailable is set during assembly init.
        // Without test-data-shared, it will be false and ShouldSkip returns true.
        // With test-data-shared, it will be true and ShouldSkip returns false.
        // Either way, it should be the inverse of IntegrationTestsAvailable.

        var attr = new SkipUnlessIntegrationAttribute();
        var shouldSkip = await attr.ShouldSkip(null!);

        await Assert.That(shouldSkip).IsEqualTo(!Utils.IntegrationTestsAvailable);
    }

    [Test]
    public async Task SkipReason_ContainsSetupInstructions()
    {
        var attr = new SkipUnlessIntegrationAttribute();

        await Assert.That(attr.Reason).Contains("LOCAL_MODEL_TESTING.md");
    }
}
