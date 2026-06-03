// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;
using System;
using System.Threading;
using System.Threading.Tasks;

public sealed class AsyncLock : IDisposable
{
    private readonly SemaphoreSlim _semaphore = new(1, 1);

    public void Dispose()
    {
        _semaphore.Dispose();
    }

    public IDisposable Lock()
    {
        _semaphore.Wait();
        return new Releaser(_semaphore);
    }

    public async Task<IDisposable> LockAsync()
    {
        await _semaphore.WaitAsync().ConfigureAwait(false);
        return new Releaser(_semaphore);
    }

    private sealed class Releaser : IDisposable
    {
        private SemaphoreSlim? _semaphore;

        public Releaser(SemaphoreSlim semaphore)
        {
            _semaphore = semaphore;
        }

        public void Dispose()
        {
            // Idempotent: only the first Dispose releases the semaphore.
            var s = Interlocked.Exchange(ref _semaphore, null);
            s?.Release();
        }
    }
}
