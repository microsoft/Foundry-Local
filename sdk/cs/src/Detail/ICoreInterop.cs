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

    internal delegate void CallbackFn(string callbackData);

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct RequestBuffer
    {
        public nint Command;
        public int CommandLength;
        public nint Data;
        public int DataLength;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ResponseBuffer
    {
        public nint Data;
        public int DataLength;
        public nint Error;
        public int ErrorLength;
    }

    // native callback function signature
    // Return: 0 = continue, 1 = cancel
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal unsafe delegate int NativeCallbackFn(nint data, int length, nint userData);

    Response ExecuteCommand(string commandName, CoreInteropRequest? commandInput = null);
    Response ExecuteCommandWithCallback(string commandName, CoreInteropRequest? commandInput, CallbackFn callback);

    Task<Response> ExecuteCommandAsync(string commandName, CoreInteropRequest? commandInput = null,
                                       CancellationToken? ct = null);
    Task<Response> ExecuteCommandWithCallbackAsync(string commandName, CoreInteropRequest? commandInput,
                                                   CallbackFn callback,
                                                   CancellationToken? ct = null);

    // --- Audio streaming session support ---

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct StreamingRequestBuffer
    {
        public nint Command;
        public int CommandLength;
        public nint Data;          // JSON params
        public int DataLength;
        public nint BinaryData;    // raw PCM audio bytes
        public int BinaryDataLength;
    }

    Response StartAudioStream(CoreInteropRequest request);
    Response PushAudioData(CoreInteropRequest request, ReadOnlyMemory<byte> audioData);
    Response StopAudioStream(CoreInteropRequest request);
}
