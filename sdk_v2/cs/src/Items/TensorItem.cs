// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public sealed class TensorItem : Item
{
    public FlTensorDataType DataType { get; }

    /// <summary>
    /// Pointer to the tensor's native data buffer. The buffer is owned by this
    /// <see cref="TensorItem"/> and remains valid only until the item is disposed
    /// (or ownership is transferred via <see cref="Item.ReleaseOwnership"/>).
    /// Callers should copy data out (e.g. via <see cref="Marshal.Copy(IntPtr, float[], int, int)"/>)
    /// before disposing; the pointer becomes invalid afterwards.
    /// </summary>
    public IntPtr Data { get; }

    public long[] Shape { get; }

    internal TensorItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var status = Api.Item.GetTensor(Ptr, out var tensor);
        Api.CheckStatus(status);

        DataType = tensor.DataType;
        Data = tensor.Data;

        var rankInt = (int)(ulong)tensor.Rank;
        Shape = new long[rankInt];
        Marshal.Copy(tensor.Shape, Shape, 0, rankInt);
    }
}
