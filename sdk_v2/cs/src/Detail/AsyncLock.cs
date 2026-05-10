// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;
using System;
using System.Threading.Tasks;

public sealed class AsyncLock : IDisposable
{
    private readonly Task<IDisposable> _releaserTask;
    private readonly SemaphoreSlim _semaphore = new(1, 1);
    private readonly IDisposable _releaser;

    public AsyncLock()
    {
        _releaser = new Releaser(_semaphore);
        _releaserTask = Task.FromResult(_releaser);
    }

    public void Dispose()
    {
        _semaphore.Dispose();
    }

    public IDisposable Lock()
    {
        _semaphore.Wait();
        return _releaser;
    }

    public Task<IDisposable> LockAsync()
    {
        Task waitTask = _semaphore.WaitAsync();

        return waitTask.IsCompleted
            ? _releaserTask
            : waitTask.ContinueWith(
                (_, releaser) => (IDisposable)releaser!,
                _releaser,
                CancellationToken.None,
                TaskContinuationOptions.ExecuteSynchronously,
                TaskScheduler.Default);
    }

    private sealed class Releaser : IDisposable
    {
        private readonly SemaphoreSlim _semaphore;

        public Releaser(SemaphoreSlim semaphore)
        {
            _semaphore = semaphore;
        }

        public void Dispose()
        {
            _semaphore.Release();
        }
    }
}
