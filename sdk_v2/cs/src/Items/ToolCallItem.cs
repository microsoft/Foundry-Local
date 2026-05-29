// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System.Runtime.InteropServices;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

namespace Microsoft.AI.Foundry.Local;

public sealed class ToolCallItem : Item
{
    public string? CallId { get; }
    public string? Name { get; }
    public string? Arguments { get; }

    public ToolCallItem(string callId, string name, string arguments)
        : base(ItemType.ToolCall)
    {
        CallId = callId;
        Name = name;
        Arguments = arguments;

        var callIdNative = Detail.Utf8.StringToCoTaskMem(callId);
        var nameNative = Detail.Utf8.StringToCoTaskMem(name);
        var argsNative = Detail.Utf8.StringToCoTaskMem(arguments);

        try
        {
            var data = new FlToolCallData
            {
                Version = NativeMethods.ApiVersion,
                CallId = callIdNative,
                Name = nameNative,
                Arguments = argsNative,
            };
            Api.CheckStatus(Api.Item.SetToolCall(Ptr, ref data));
        }
        finally
        {
            Marshal.FreeCoTaskMem(callIdNative);
            Marshal.FreeCoTaskMem(nameNative);
            Marshal.FreeCoTaskMem(argsNative);
        }
    }

    internal ToolCallItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
        var status = Api.Item.GetToolCall(Ptr, out var toolCall);
        Api.CheckStatus(status);
        CallId = Detail.Utf8.PtrToString(toolCall.CallId);
        Name = Detail.Utf8.PtrToString(toolCall.Name);
        Arguments = Detail.Utf8.PtrToString(toolCall.Arguments);
    }
}
