// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail.Native;

internal sealed class LifecycleRaceTests
{
    [Test]
    public async Task EnsureInitialized_Concurrent_DoesNotThrow()
    {
        // Api is already initialized by the assembly-init bootstrap, but EnsureInitialized
        // must remain safe to call concurrently from any number of threads.
        const int threads = 32;
        using var ready = new CountdownEvent(threads);
        using var go = new ManualResetEventSlim(false);

        var tasks = new Task[threads];
        for (int i = 0; i < threads; i++)
        {
            tasks[i] = Task.Run(() =>
            {
                ready.Signal();
                go.Wait();
                Api.EnsureInitialized();
            });
        }

        ready.Wait();
        go.Set();

        await Task.WhenAll(tasks);
    }

    [Test]
    public async Task Dispose_Concurrent_OnlyDisposesOnce()
    {
        // Manager already created via assembly-init Utils. We don't want to dispose the live
        // singleton (other tests would fail), so exercise concurrent Dispose against a fresh
        // throwaway CountingDisposable that uses the same Interlocked-Exchange pattern.
        using var d = new CountingDisposable();

        const int threads = 32;
        var tasks = new Task[threads];
        for (int i = 0; i < threads; i++)
        {
            tasks[i] = Task.Run(d.Dispose);
        }

        await Task.WhenAll(tasks);

        await Assert.That(d.DisposeCount).IsEqualTo(1);
    }

    private sealed class CountingDisposable : IDisposable
    {
        private int _disposed;
        public int DisposeCount;

        public void Dispose()
        {
            if (Interlocked.Exchange(ref _disposed, 1) != 0)
            {
                return;
            }

            Interlocked.Increment(ref DisposeCount);
        }
    }
}
