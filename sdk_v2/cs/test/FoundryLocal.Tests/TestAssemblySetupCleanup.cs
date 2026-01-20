// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;
using System.Threading.Tasks;

internal static class TestAssemblySetupCleanup
{

    [After(Assembly)]
    public static async Task Cleanup(AssemblyHookContext _)
    {
        try
        {
            // ensure any loaded models are unloaded
            var manager = FoundryLocalManager.Instance; // initialized by Utils
            var catalog = await manager.GetCatalogAsync();
            var models = await catalog.GetLoadedModelsAsync().ConfigureAwait(false);

            foreach (var model in models)
            {
                await Assert.That(await model.IsLoadedAsync()).IsTrue();
                await model.UnloadAsync().ConfigureAwait(false);
                await Assert.That(await model.IsLoadedAsync()).IsFalse();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error during Cleanup: {ex}");
            throw;
        }
    }
}
