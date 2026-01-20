// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;


internal interface ICoreInterop
{
    internal record Response
    {
        internal string? Data;
        internal string? Error;
    }

    public delegate void CallbackFn(string callbackData);

    [StructLayout(LayoutKind.Sequential)]
    protected unsafe struct RequestBuffer
    {
        public nint Command;
        public int CommandLength;
        public nint Data;
        public int DataLength;
    }

    [StructLayout(LayoutKind.Sequential)]
    protected unsafe struct ResponseBuffer
    {
        public nint Data;
        public int DataLength;
        public nint Error;
        public int ErrorLength;
    }

    // native callback function signature
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    protected unsafe delegate void NativeCallbackFn(nint data, int length, nint userData);

    Response ExecuteCommand(string commandName, CoreInteropRequest? commandInput = null);
    Response ExecuteCommandWithCallback(string commandName, CoreInteropRequest? commandInput, CallbackFn callback);

    Task<Response> ExecuteCommandAsync(string commandName, CoreInteropRequest? commandInput = null,
                                       CancellationToken? ct = null);
    Task<Response> ExecuteCommandWithCallbackAsync(string commandName, CoreInteropRequest? commandInput,
                                                   CallbackFn callback,
                                                   CancellationToken? ct = null);
}
