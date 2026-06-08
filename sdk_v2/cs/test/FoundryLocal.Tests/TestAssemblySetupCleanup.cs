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
            // if running individual test/s they may not have used the Utils class which creates FoundryLocalManager
            if (FoundryLocalManager.IsInitialized)
            {
                var manager = FoundryLocalManager.Instance; // initialized by Utils

                // Shutdown is the authoritative drain: rejects new loads, cancels and waits
                // for HTTP-tracked sessions, then unloads every loaded model (waiting per-model
                // for its session refcount to reach zero, with a bounded timeout). This replaces
                // the previous per-class UnloadAsync loop, which raced with parallel test classes.
                await Assert.That(manager.IsShutdownRequested).IsFalse();
                manager.Shutdown();
                await Assert.That(manager.IsShutdownRequested).IsTrue();
                // Idempotent — calling again must not throw.
                manager.Shutdown();
                await Assert.That(manager.IsShutdownRequested).IsTrue();

                // After Shutdown, no models should remain loaded. If any do, a stuck caller
                // held a session past the drain timeout — log so it's visible.
                var catalog = await manager.GetCatalogAsync();
                var stillLoaded = await catalog.GetLoadedModelsAsync().ConfigureAwait(false);
                if (stillLoaded.Count > 0)
                {
                    var ids = string.Join(", ", stillLoaded.Select(m => m.Id));
                    Console.WriteLine($"Cleanup: {stillLoaded.Count} model(s) still loaded after Shutdown: {ids}");
                }

                // Dispose the manager to release native resources and prevent DLL locking
                manager.Dispose();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error during Cleanup: {ex}");
            throw;
        }
    }
}
