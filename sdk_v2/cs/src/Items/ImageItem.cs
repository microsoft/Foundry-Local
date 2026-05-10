// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

#pragma warning disable IDISP001
#pragma warning disable IDISP023

namespace Microsoft.AI.Foundry.Local;

public sealed class ImageItem : Item
{
    private IntPtr _data;
    private int _dataSize;
    private PinContext? _pinContext;  // held for borrowed path; null for owned/URI/native

    // Static deleter thunk — created once, never collected
    private static readonly FlImageDataDeleterDelegate s_deleter = ImageDeleter;
    private static readonly IntPtr s_deleterPtr = Marshal.GetFunctionPointerForDelegate(s_deleter);

    /// <summary>Raw image bytes. Only valid while this item is alive. Empty for URI-based items.</summary>
    public ReadOnlySpan<byte> Data
    {
        get
        {
            unsafe { return new ReadOnlySpan<byte>((void*)_data, _dataSize); }
        }
    }

    public string? Format { get; private set; }
    public string? Uri { get; private set; }

    /// <summary>Create from raw bytes. format is e.g. "png".</summary>
    public ImageItem(string format, ReadOnlyMemory<byte> data) : base(ItemType.Image)
    {
        Format = format;
        Uri = null;

        _pinContext = PinContext.Pin(data);
        SetNativeImage(_pinContext.Pointer, _pinContext.Length, mutableData: IntPtr.Zero, format);

        _data = _pinContext.Pointer;
        _dataSize = _pinContext.Length;
    }

    /// <summary>
    /// Create from mutable borrowed data. The item pins the memory until disposed.
    /// Native code may write into the buffer.
    /// </summary>
    public ImageItem(string format, Memory<byte> data) : base(ItemType.Image)
    {
        Format = format;
        Uri = null;

        _pinContext = PinContext.Pin(data);
        SetNativeImage(_pinContext.Pointer, _pinContext.Length, mutableData: _pinContext.Pointer, format);

        _data = _pinContext.Pointer;
        _dataSize = _pinContext.Length;
    }

    /// <summary>Create from a URI (file path, URL, etc.).</summary>
    public ImageItem(string uri, string? format = null) : base(ItemType.Image)
    {
        Uri = uri;
        Format = format;
        _data = IntPtr.Zero;
        _dataSize = 0;

        var uriNative = Marshal.StringToCoTaskMemUTF8(uri);
        var formatNative = format != null ? Marshal.StringToCoTaskMemUTF8(format) : IntPtr.Zero;

        try
        {
            var imageData = new FlImageData
            {
                Version = NativeMethods.ApiVersion,
                Data = IntPtr.Zero,
                MutableData = IntPtr.Zero,
                DataSize = UIntPtr.Zero,
                Format = formatNative,
                Uri = uriNative,
                Deleter = IntPtr.Zero,
                DeleterUserData = IntPtr.Zero,
            };
            Api.CheckStatus(Api.Item.SetImage(Ptr, ref imageData));
        }
        finally
        {
            Marshal.FreeCoTaskMem(uriNative);

            if (formatNative != IntPtr.Zero)
            {
                Marshal.FreeCoTaskMem(formatNative);
            }
        }
    }

    /// <summary>
    /// Create from read-only owned data. The native item takes ownership via a pin that
    /// survives past Dispose(). Safe for ItemQueue push and ownership transfer.
    /// </summary>
    public static ImageItem CreateOwned(string format, ReadOnlyMemory<byte> data)
    {
        var item = new ImageItem(ItemType.Image);
        item.Format = format;

        var pinCtx = PinContext.Pin(data);
        var userData = pinCtx.AllocForNativeDeleter();

        // mutable_data must be non-NULL when deleter is set (C API contract)
        item.SetNativeImageOwned(pinCtx.Pointer, pinCtx.Length, pinCtx.Pointer, format, s_deleterPtr, userData);

        item._data = pinCtx.Pointer;
        item._dataSize = pinCtx.Length;
        return item;
    }

    /// <summary>
    /// Create from mutable owned data. The native item takes ownership via a pin that
    /// survives past Dispose(). Safe for ItemQueue push and ownership transfer.
    /// </summary>
    public static ImageItem CreateOwned(string format, Memory<byte> data)
    {
        var item = new ImageItem(ItemType.Image);
        item.Format = format;

        var pinCtx = PinContext.Pin(data);
        var userData = pinCtx.AllocForNativeDeleter();

        item.SetNativeImageOwned(pinCtx.Pointer, pinCtx.Length, pinCtx.Pointer, format, s_deleterPtr, userData);

        item._data = pinCtx.Pointer;
        item._dataSize = pinCtx.Length;
        return item;
    }

    /// <summary>Wrap an existing native image item (from Response, Queue, etc.).</summary>
    internal ImageItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var status = Api.Item.GetImage(Ptr, out var image);
        Api.CheckStatus(status);
        _data = image.Data;
        _dataSize = (int)(ulong)image.DataSize;
        Format = Api.Utf8(image.Format);
        Uri = Api.Utf8(image.Uri);
    }

    // Private constructor for static factories — creates native item but sets no data yet
    private ImageItem(ItemType type) : base(type)
    {
    }

    private void SetNativeImage(IntPtr dataPtr, int dataSize, IntPtr mutableData, string format)
    {
        var formatNative = Marshal.StringToCoTaskMemUTF8(format);

        try
        {
            var imageData = new FlImageData
            {
                Version = NativeMethods.ApiVersion,
                Data = dataPtr,
                MutableData = mutableData,
                DataSize = (UIntPtr)dataSize,
                Format = formatNative,
                Uri = IntPtr.Zero,
                Deleter = IntPtr.Zero,
                DeleterUserData = IntPtr.Zero,
            };
            Api.CheckStatus(Api.Item.SetImage(Ptr, ref imageData));
        }
        finally
        {
            Marshal.FreeCoTaskMem(formatNative);
        }
    }

    private void SetNativeImageOwned(IntPtr dataPtr, int dataSize, IntPtr mutableData,
                                     string format, IntPtr deleter, IntPtr deleterUserData)
    {
        var formatNative = Marshal.StringToCoTaskMemUTF8(format);

        try
        {
            var imageData = new FlImageData
            {
                Version = NativeMethods.ApiVersion,
                Data = dataPtr,
                MutableData = mutableData,
                DataSize = (UIntPtr)dataSize,
                Format = formatNative,
                Uri = IntPtr.Zero,
                Deleter = deleter,
                DeleterUserData = deleterUserData,
            };
            Api.CheckStatus(Api.Item.SetImage(Ptr, ref imageData));
        }
        finally
        {
            Marshal.FreeCoTaskMem(formatNative);
        }
    }

    private static void ImageDeleter(ref FlImageData data, IntPtr userData)
    {
        PinContext.ReleaseFromNative(userData);
    }

    protected override void OnDisposing()
    {
        _pinContext?.Dispose();
        _pinContext = null;
    }
}
