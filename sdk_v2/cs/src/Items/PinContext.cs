// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Buffers;
using System.Runtime.InteropServices;

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// Manages pinning of Memory/ReadOnlyMemory buffers for native interop.
/// Borrowed items hold a PinContext and dispose it. Owned items transfer
/// the PinContext to a GCHandle that the native deleter releases.
/// </summary>
internal sealed class PinContext : IDisposable
{
    private MemoryHandle _handle;

    /// <summary>Stable pointer to the pinned data.</summary>
    public IntPtr Pointer { get; }

    /// <summary>Byte count.</summary>
    public int Length { get; }

    private PinContext(MemoryHandle handle, IntPtr pointer, int length)
    {
        _handle = handle;
        Pointer = pointer;
        Length = length;
    }

    public static PinContext Pin(ReadOnlyMemory<byte> data)
    {
        var handle = data.Pin();
        IntPtr ptr;

        unsafe
        {
            ptr = (IntPtr)handle.Pointer;
        }

        return new PinContext(handle, ptr, data.Length);
    }

    public static PinContext Pin(Memory<byte> data)
        => Pin((ReadOnlyMemory<byte>)data);

    /// <summary>
    /// Allocate a GCHandle so the native deleter can find this PinContext.
    /// Returns the IntPtr to pass as deleter_user_data.
    /// After this call, the PinContext is kept alive by the GCHandle — the caller
    /// should NOT store or dispose it.
    /// </summary>
    public IntPtr AllocForNativeDeleter()
    {
        var gcHandle = GCHandle.Alloc(this, GCHandleType.Normal);
        return GCHandle.ToIntPtr(gcHandle);
    }

    /// <summary>
    /// Called by native deleter thunks. Unpins the memory and frees the GCHandle.
    /// </summary>
    public static void ReleaseFromNative(IntPtr userData)
    {
        var gcHandle = GCHandle.FromIntPtr(userData);
        var ctx = (PinContext)gcHandle.Target!;
        ctx._handle.Dispose();
        gcHandle.Free();
    }

    /// <summary>For borrowed path: unpin in Item.Dispose().</summary>
    public void Dispose()
    {
        _handle.Dispose();
    }
}
