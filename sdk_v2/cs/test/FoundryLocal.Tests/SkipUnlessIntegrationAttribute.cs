// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;

using TUnit.Core;

/// <summary>
/// Skips the test when the integration-test infrastructure (a real
/// <see cref="FoundryLocalManager"/> backed by a populated model cache) is not available.
///
/// <see cref="Utils.IntegrationTestsAvailable"/> is set during assembly init:
/// true when the shared test-data directory exists AND the manager initialized
/// successfully; false otherwise. Tests that need a live manager — anything that
/// loads a model, talks to the catalog, or runs inference — should be marked with
/// this attribute so they degrade gracefully when only the build outputs (no models,
/// no native services) are available.
/// </summary>
public class SkipUnlessIntegrationAttribute()
    : SkipAttribute("Integration test infrastructure not available. See LOCAL_MODEL_TESTING.md for setup instructions.")
{
    public override Task<bool> ShouldSkip(TestRegisteredContext context)
    {
        var integrationTestsAvailable = Utils.IntegrationTestsAvailable;
        return Task.FromResult(!integrationTestsAvailable);
    }
}
