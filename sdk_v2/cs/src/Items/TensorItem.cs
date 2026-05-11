// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public sealed class TensorItem : Item
{
    public FlTensorDataType DataType { get; }
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
