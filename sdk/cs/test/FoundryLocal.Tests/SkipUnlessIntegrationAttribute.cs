// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using TUnit.Core;

using System.Threading.Tasks;

public class SkipUnlessIntegrationAttribute()
    : SkipAttribute("Integration test infrastructure not available. See LOCAL_MODEL_TESTING.md for setup instructions.")
{
    public override Task<bool> ShouldSkip(TestRegisteredContext context)
    {
        return Task.FromResult(!Utils.IntegrationTestsAvailable);
    }
}
