// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

// Modern .NET (net7.0+) native library loading and source-generated P/Invoke declarations.

#if NET7_0_OR_GREATER

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

using static Microsoft.AI.Foundry.Local.Detail.ICoreInterop;

internal partial class CoreInterop
{
    [LibraryImport(LibraryName, EntryPoint = "execute_command")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreExecuteCommand(RequestBuffer* request, ResponseBuffer* response);

    [LibraryImport(LibraryName, EntryPoint = "execute_command_with_callback")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreExecuteCommandWithCallback(RequestBuffer* nativeRequest,
                                                                      ResponseBuffer* nativeResponse,
                                                                      nint callbackPtr,
                                                                      nint userData);

    [LibraryImport(LibraryName, EntryPoint = "execute_command_with_binary")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreExecuteCommandWithBinary(StreamingRequestBuffer* nativeRequest,
                                                                     ResponseBuffer* nativeResponse);

    [LibraryImport(LibraryName, EntryPoint = "audio_stream_start")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreAudioStreamStart(RequestBuffer* request, ResponseBuffer* response);

    [LibraryImport(LibraryName, EntryPoint = "audio_stream_push")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreAudioStreamPush(StreamingRequestBuffer* request, ResponseBuffer* response);

    [LibraryImport(LibraryName, EntryPoint = "audio_stream_stop")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreAudioStreamStop(RequestBuffer* request, ResponseBuffer* response);

    private static bool TryLoadNativeLibrary(string path, out IntPtr handle)
    {
        return NativeLibrary.TryLoad(path, out handle);
    }

    static partial void InitializeNativeLibraryResolver()
    {
        NativeLibrary.SetDllImportResolver(typeof(CoreInterop).Assembly, (libraryName, assembly, searchPath) =>
        {
            if (libraryName == LibraryName)
            {
                Debug.WriteLine($"Resolving {libraryName}. BaseDirectory: {AppContext.BaseDirectory}");

                // Check if this build is platform specific. In that case all files are flattened
                // in the one directory and there's no need to look in runtimes/<os>-<arch>/native.
                // e.g. `dotnet publish -r win-x64` copies all dependencies into the publish output folder.
                var libraryPath = Path.Combine(AppContext.BaseDirectory, AddLibraryExtension(LibraryName));
                if (File.Exists(libraryPath))
                {
                    if (NativeLibrary.TryLoad(libraryPath, out var handle))
                    {
                        Debug.WriteLine($"Loaded native library from: {libraryPath}");

                        if (IsWindows)
                        {
                            LoadOrtDllsIfInSameDir(AppContext.BaseDirectory);
                        }

                        return handle;
                    }
                }

                // TODO: figure out what is required on Android and iOS
                // The nuget has an AAR and xcframework respectively so we need to determine what files are where
                // after a build.
                var os = IsWindows ? "win" :
                         IsLinux ? "linux" :
                         IsMacOS ? "osx" :
                         throw new PlatformNotSupportedException();

                var arch = RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant();
                var runtimePath = Path.Combine(AppContext.BaseDirectory, "runtimes", $"{os}-{arch}", "native");
                libraryPath = Path.Combine(runtimePath, AddLibraryExtension(LibraryName));

                Debug.WriteLine($"Looking for native library at: {libraryPath}");

                if (File.Exists(libraryPath))
                {
                    if (NativeLibrary.TryLoad(libraryPath, out var handle))
                    {
                        Debug.WriteLine($"Loaded native library from: {libraryPath}");

                        if (IsWindows)
                        {
                            LoadOrtDllsIfInSameDir(runtimePath);
                        }

                        return handle;
                    }
                }
            }

            return IntPtr.Zero;
        });
    }
}

#endif
