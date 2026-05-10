// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public sealed class Request : IDisposable
{
    internal IntPtr Ptr { get; private set; }
    private bool _disposed;

    public Request()
    {
        Api.EnsureInitialized();
        var status = Api.Inference.RequestCreate(out var ptr);
        Api.CheckStatus(status);
        Ptr = ptr;
    }

    /// <summary>Add an item to the request. Transfers ownership — do not use the item after this.</summary>
    public Request AddItem(Item item)
    {
        var nativePtr = item.ReleaseOwnership();
        Api.CheckStatus(Api.Inference.RequestAddItem(Ptr, nativePtr, true));
        return this;
    }

    public int ItemCount => (int)(ulong)Api.Inference.RequestGetItemCount(Ptr);

    public Item GetItem(int index)
    {
        var status = Api.Inference.RequestGetItem(Ptr, (UIntPtr)index, out var itemPtr);
        Api.CheckStatus(status);
        return Item.FromNative(itemPtr, ownsHandle: false);
    }

    /// <summary>
    /// Set per-request inference options. Use <see cref="SessionParam"/> constants for well-known keys.
    /// Per-request options override session-level options for this request only.
    /// </summary>
    public Request SetOptions(IDictionary<string, string> options)
    {
        Api.Root.CreateKeyValuePairs(out var kvpPtr);

        try
        {
            foreach (var kvp in options)
            {
                Api.Root.AddKeyValuePair(kvpPtr, kvp.Key, kvp.Value);
            }

            Api.CheckStatus(Api.Inference.RequestSetOptions(Ptr, kvpPtr));
        }
        finally
        {
            Api.Root.KeyValuePairsRelease(kvpPtr);
        }

        return this;
    }

    internal Request SetOptions(IntPtr options)
    {
        Api.CheckStatus(Api.Inference.RequestSetOptions(Ptr, options));
        return this;
    }

    public void Cancel()
    {
        Api.CheckStatus(Api.Inference.RequestCancel(Ptr));
    }

    public void Dispose()
    {
        if (!_disposed && Ptr != IntPtr.Zero)
        {
            Api.Inference.RequestRelease(Ptr);
            Ptr = IntPtr.Zero;
            _disposed = true;
        }
    }
}
