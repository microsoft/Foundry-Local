// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Collections;
using System.Collections.Generic;

using Microsoft.AI.Foundry.Local.Detail.Native;

public sealed class Response : IDisposable, IEnumerable<Item>
{
    internal IntPtr Ptr { get; private set; }
    private bool _disposed;

    internal Response(IntPtr ptr)
    {
        Ptr = ptr;
    }

    public int ItemCount => (int)(ulong)Api.Inference.ResponseGetItemCount(Ptr);

    /// <summary>
    /// Returns a non-owning view of the item at <paramref name="index"/>. The returned
    /// <see cref="Item"/> wraps a native handle owned by this <see cref="Response"/>; it
    /// is valid only while this <see cref="Response"/> is alive. Do not call
    /// <see cref="Item.Dispose"/> on it (the call is a no-op for non-owning items but is
    /// misleading), and do not retain or use it after the parent <see cref="Response"/>
    /// has been disposed.
    /// </summary>
    public Item GetItem(int index)
    {
        var status = Api.Inference.ResponseGetItem(Ptr, (UIntPtr)index, out var itemPtr);
        Api.CheckStatus(status);
        return Item.FromNative(itemPtr, ownsHandle: false);
    }

    public FinishReason FinishReason => (FinishReason)Api.Inference.ResponseGetFinishReason(Ptr);

    public TokenUsage GetUsage()
    {
        var status = Api.Inference.ResponseGetUsage(Ptr, out var usage);
        Api.CheckStatus(status);
        return new TokenUsage((int)usage.PromptTokens, (int)usage.CompletionTokens, (int)usage.TotalTokens);
    }

    /// <summary>
    /// Enumerates non-owning <see cref="Item"/> views over this response's items. The
    /// yielded items share the same lifetime as this <see cref="Response"/> — they must
    /// not be disposed and must not be used after the response is disposed.
    /// </summary>
    public IEnumerator<Item> GetEnumerator()
    {
        for (int i = 0; i < ItemCount; i++)
        {
            yield return GetItem(i);
        }
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    public void Dispose()
    {
        if (!_disposed && Ptr != IntPtr.Zero)
        {
            Api.Inference.ResponseRelease(Ptr);
            Ptr = IntPtr.Zero;
            _disposed = true;
        }
    }
}
