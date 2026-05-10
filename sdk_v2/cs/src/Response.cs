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
