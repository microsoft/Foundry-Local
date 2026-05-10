// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public sealed class ToolResultItem : Item
{
    public string? CallId { get; }
    public string? Result { get; }

    public ToolResultItem(string callId, string result) : base(ItemType.ToolResult)
    {
        CallId = callId;
        Result = result;

        var callIdNative = Marshal.StringToCoTaskMemUTF8(callId);
        var resultNative = Marshal.StringToCoTaskMemUTF8(result);

        try
        {
            var data = new FlToolResultData
            {
                Version = NativeMethods.ApiVersion,
                CallId = callIdNative,
                Result = resultNative,
            };
            Api.CheckStatus(Api.Item.SetToolResult(Ptr, ref data));
        }
        finally
        {
            Marshal.FreeCoTaskMem(callIdNative);
            Marshal.FreeCoTaskMem(resultNative);
        }
    }

    internal ToolResultItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var status = Api.Item.GetToolResult(Ptr, out var toolResult);
        Api.CheckStatus(status);
        CallId = Api.Utf8(toolResult.CallId);
        Result = Api.Utf8(toolResult.Result);
    }
}
