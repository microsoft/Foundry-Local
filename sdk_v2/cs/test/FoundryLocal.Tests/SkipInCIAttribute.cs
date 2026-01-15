// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using TUnit.Core;

using System.Threading.Tasks;

public class SkipInCIAttribute() : SkipAttribute("This test is only supported locally. Skipped on CIs.")
{
    public override Task<bool> ShouldSkip(TestRegisteredContext context)
    {
        return Task.FromResult(Utils.IsRunningInCI());
    }
}
