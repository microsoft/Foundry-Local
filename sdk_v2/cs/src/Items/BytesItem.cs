// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

#pragma warning disable IDISP001
#pragma warning disable IDISP023

namespace Microsoft.AI.Foundry.Local;

public sealed class BytesItem : Item
{
    private IntPtr _data;
    private int _dataSize;
    private PinContext? _pinContext;

    // Static deleter thunk — created once, never collected
    private static readonly FlBytesDataDeleterDelegate s_deleter = BytesDeleter;
    private static readonly IntPtr s_deleterPtr = Marshal.GetFunctionPointerForDelegate(s_deleter);

    /// <summary>Raw bytes. Only valid while this item is alive.</summary>
    public ReadOnlySpan<byte> Data
    {
        get
        {
            unsafe { return new ReadOnlySpan<byte>((void*)_data, _dataSize); }
        }
    }

    /// <summary>Create from read-only borrowed data. The item pins the memory until disposed.</summary>
    public BytesItem(ReadOnlyMemory<byte> data) : base(ItemType.Bytes)
    {
        _pinContext = PinContext.Pin(data);
        SetNativeBytes(_pinContext.Pointer, _pinContext.Length, mutableData: IntPtr.Zero);

        _data = _pinContext.Pointer;
        _dataSize = _pinContext.Length;
    }

    /// <summary>Create from mutable borrowed data. The item pins the memory until disposed.</summary>
    public BytesItem(Memory<byte> data) : base(ItemType.Bytes)
    {
        _pinContext = PinContext.Pin(data);
        SetNativeBytes(_pinContext.Pointer, _pinContext.Length, mutableData: _pinContext.Pointer);

        _data = _pinContext.Pointer;
        _dataSize = _pinContext.Length;
    }

    /// <summary>Create from read-only owned data. Safe for ItemQueue push.</summary>
    public static BytesItem CreateOwned(ReadOnlyMemory<byte> data)
    {
        var item = new BytesItem(ItemType.Bytes);

        var pinCtx = PinContext.Pin(data);
        var userData = pinCtx.AllocForNativeDeleter();

        // mutable_data must be non-NULL when deleter is set (C API contract)
        item.SetNativeBytesOwned(pinCtx.Pointer, pinCtx.Length, pinCtx.Pointer, s_deleterPtr, userData);

        item._data = pinCtx.Pointer;
        item._dataSize = pinCtx.Length;
        return item;
    }

    /// <summary>Create from mutable owned data. Safe for ItemQueue push.</summary>
    public static BytesItem CreateOwned(Memory<byte> data)
    {
        var item = new BytesItem(ItemType.Bytes);

        var pinCtx = PinContext.Pin(data);
        var userData = pinCtx.AllocForNativeDeleter();

        item.SetNativeBytesOwned(pinCtx.Pointer, pinCtx.Length, pinCtx.Pointer, s_deleterPtr, userData);

        item._data = pinCtx.Pointer;
        item._dataSize = pinCtx.Length;
        return item;
    }

    /// <summary>Wrap an existing native bytes item (from Response, Queue, etc.).</summary>
    internal BytesItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var status = Api.Item.GetBytes(Ptr, out var bytes);
        Api.CheckStatus(status);
        _data = bytes.Data;
        _dataSize = (int)(ulong)bytes.DataSize;
    }

    // Private constructor for static factories — creates native item but sets no data yet
    private BytesItem(ItemType type) : base(type)
    {
    }

    private void SetNativeBytes(IntPtr dataPtr, int dataSize, IntPtr mutableData)
    {
        var bytesData = new FlBytesData
        {
            Version = NativeMethods.ApiVersion,
            ItemType = FlItemType.Bytes,
            Data = dataPtr,
            MutableData = mutableData,
            DataSize = (UIntPtr)dataSize,
            Deleter = IntPtr.Zero,
            DeleterUserData = IntPtr.Zero,
        };
        Api.CheckStatus(Api.Item.SetBytes(Ptr, ref bytesData));
    }

    private void SetNativeBytesOwned(IntPtr dataPtr, int dataSize, IntPtr mutableData,
                                     IntPtr deleter, IntPtr deleterUserData)
    {
        var bytesData = new FlBytesData
        {
            Version = NativeMethods.ApiVersion,
            ItemType = FlItemType.Bytes,
            Data = dataPtr,
            MutableData = mutableData,
            DataSize = (UIntPtr)dataSize,
            Deleter = deleter,
            DeleterUserData = deleterUserData,
        };
        Api.CheckStatus(Api.Item.SetBytes(Ptr, ref bytesData));
    }

    private static void BytesDeleter(ref FlBytesData data, IntPtr userData)
    {
        PinContext.ReleaseFromNative(userData);
    }

    protected override void OnDisposing()
    {
        _pinContext?.Dispose();
        _pinContext = null;
    }
}
