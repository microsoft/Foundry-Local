// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

// Legacy native library loading and runtime-generated P/Invoke declarations for
// .NET Framework 4.6.2+ (Windows only). LoadLibraryW pre-loads the native DLL
// so [DllImport] can resolve it by name from the process module table.

#if !NET7_0_OR_GREATER

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

using static Microsoft.AI.Foundry.Local.Detail.ICoreInterop;

internal partial class CoreInterop
{
    [DllImport(LibraryName, EntryPoint = "execute_command", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreExecuteCommand(RequestBuffer* request, ResponseBuffer* response);

    [DllImport(LibraryName, EntryPoint = "execute_command_with_callback", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreExecuteCommandWithCallback(RequestBuffer* nativeRequest,
                                                                     ResponseBuffer* nativeResponse,
                                                                     nint callbackPtr,
                                                                     nint userData);

    [DllImport(LibraryName, EntryPoint = "execute_command_with_binary", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreExecuteCommandWithBinary(StreamingRequestBuffer* nativeRequest,
                                                                    ResponseBuffer* nativeResponse);

    [DllImport(LibraryName, EntryPoint = "audio_stream_start", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreAudioStreamStart(RequestBuffer* request, ResponseBuffer* response);

    [DllImport(LibraryName, EntryPoint = "audio_stream_push", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreAudioStreamPush(StreamingRequestBuffer* request, ResponseBuffer* response);

    [DllImport(LibraryName, EntryPoint = "audio_stream_stop", CallingConvention = CallingConvention.Cdecl)]
    private static unsafe extern void CoreAudioStreamStop(RequestBuffer* request, ResponseBuffer* response);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryW(string path);

    private static bool TryLoadNativeLibrary(string path, out IntPtr handle)
    {
        handle = LoadLibraryW(path);
        return handle != IntPtr.Zero;
    }

    static partial void InitializeNativeLibraryResolver()
    {
        if (!IsWindows)
        {
            throw new PlatformNotSupportedException(
                "The netstandard2.0 build is only supported on .NET Framework 4.6.2+ (Windows). " +
                "Use the net8.0 build for cross-platform support.");
        }

        // Pre-load the native library using the same path probing as the net8.0 resolver.
        // Once loaded, [DllImport] finds it by name in the process module table.
        var libraryPath = Path.Combine(AppContext.BaseDirectory, AddLibraryExtension(LibraryName));
        Debug.WriteLine($"Resolving {LibraryName}. BaseDirectory: {AppContext.BaseDirectory}");

        if (File.Exists(libraryPath))
        {
            if (TryLoadNativeLibrary(libraryPath, out _))
            {
                Debug.WriteLine($"Loaded native library from: {libraryPath}");
                LoadOrtDllsIfInSameDir(AppContext.BaseDirectory);
                return;
            }
        }

        var arch = RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant();
        var runtimePath = Path.Combine(AppContext.BaseDirectory, "runtimes", $"win-{arch}", "native");
        libraryPath = Path.Combine(runtimePath, AddLibraryExtension(LibraryName));

        Debug.WriteLine($"Looking for native library at: {libraryPath}");

        if (File.Exists(libraryPath))
        {
            if (TryLoadNativeLibrary(libraryPath, out _))
            {
                Debug.WriteLine($"Loaded native library from: {libraryPath}");
                LoadOrtDllsIfInSameDir(runtimePath);
            }
        }
    }
}

#endif
