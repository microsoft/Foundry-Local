// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

// Suppress IDisposableAnalyzers warnings for the Item base:
// - IDISP023: Dispose accesses static Api which is always available
// - IDISP025: Subclasses are sealed with no extra native resources — virtual dispose not needed
#pragma warning disable IDISP023
#pragma warning disable IDISP025

namespace Microsoft.AI.Foundry.Local;

public class Item : IDisposable
{
    internal IntPtr Ptr { get; private set; }
    private bool _ownsHandle;
    private bool _disposed;

    internal Item(IntPtr ptr, bool ownsHandle)
    {
        Ptr = ptr;
        _ownsHandle = ownsHandle;
    }

    protected Item(ItemType type)
    {
        Api.EnsureInitialized();
        var status = Api.Item.Create((FlItemType)type, out var ptr);
        Api.CheckStatus(status);
        Ptr = ptr;
        _ownsHandle = true;
    }

    public ItemType ItemType => (ItemType)Api.Item.GetType(Ptr);

    internal static Item FromNative(IntPtr ptr, bool ownsHandle)
    {
        var type = (ItemType)Api.Item.GetType(ptr);

        return type switch
        {
            ItemType.Bytes      => new BytesItem(ptr, ownsHandle),
            ItemType.Text       => new TextItem(ptr, ownsHandle),
            ItemType.Message    => new MessageItem(ptr, ownsHandle),
            ItemType.Image      => new ImageItem(ptr, ownsHandle),
            ItemType.Audio      => new AudioItem(ptr, ownsHandle),
            ItemType.ToolCall   => new ToolCallItem(ptr, ownsHandle),
            ItemType.ToolResult => new ToolResultItem(ptr, ownsHandle),
            ItemType.Tensor     => new TensorItem(ptr, ownsHandle),
            _                   => new Item(ptr, ownsHandle),
        };
    }

    /// <summary>
    /// Transfer ownership to the caller (e.g., before adding to a Request).
    /// After this call the item will no longer release the native handle.
    /// </summary>
    internal IntPtr ReleaseOwnership()
    {
        _ownsHandle = false;
        return Ptr;
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            if (_ownsHandle && Ptr != IntPtr.Zero)
            {
                Api.Item.Release(Ptr);
                Ptr = IntPtr.Zero;
            }

            OnDisposing();
            _disposed = true;
        }

        GC.SuppressFinalize(this);
    }

    /// <summary>
    /// Called during Dispose after the native handle is released.
    /// Override in subclasses to release additional resources (e.g., unpin borrowed memory).
    /// </summary>
    protected virtual void OnDisposing()
    {
    }
}
