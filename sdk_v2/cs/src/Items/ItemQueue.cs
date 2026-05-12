// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// A queue of items used for streaming. Items can be pushed and popped.
/// </summary>
public sealed class ItemQueue : IDisposable
{
    private IntPtr _ptr;
    private bool _disposed;

    public ItemQueue()
    {
        Api.EnsureInitialized();
        var status = Api.Item.QueueCreate(out _ptr);
        Api.CheckStatus(status);
    }

    internal IntPtr Ptr => _ptr;

    /// <summary>Push an item into the queue. Transfers ownership of the item.</summary>
    public void Push(Item item)
    {
        var nativePtr = item.ReleaseOwnership();
        Api.CheckStatus(Api.Item.QueuePush(_ptr, nativePtr));
    }

    /// <summary>Try to pop an item from the queue. Returns null if queue is empty.</summary>
    public Item? TryPop()
    {
        if (Api.Item.QueueTryPop(_ptr, out var itemPtr))
        {
            return Item.FromNative(itemPtr, ownsHandle: true);
        }

        return null;
    }

    public int Count => checked((int)(ulong)Api.Item.QueueSize(_ptr));

    public void MarkFinished() => Api.Item.QueueMarkFinished(_ptr);

    public bool IsFinished => Api.Item.QueueGetFinished(_ptr);

    public void Dispose()
    {
        if (!_disposed && _ptr != IntPtr.Zero)
        {
            Api.Item.QueueRelease(_ptr);
            _ptr = IntPtr.Zero;
            _disposed = true;
        }
    }
}
