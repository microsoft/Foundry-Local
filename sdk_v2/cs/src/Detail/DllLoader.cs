// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Runtime.InteropServices;
using Microsoft.AI.Foundry.Local.Detail.Interop;

/// <summary>
/// Handles native DLL resolution for foundry_local.dll.
/// ORT dependencies (onnxruntime.dll, onnxruntime-genai.dll) are loaded by the native
/// Manager constructor via RuntimeLibraryPath — the managed side does not pre-load them.
/// Must be initialized before any P/Invoke calls to the native library.
/// </summary>
internal static class DllLoader
{
    private static bool _initialized;
    private static readonly object _lock = new();

    // Name of the redirect file written by the csproj when FoundryLocalNativeBinDir is set.
    // Contains a single line: the absolute path to the C++ build output directory.
    private const string RedirectFileName = "foundry_local.native.cfg";

    internal static void Initialize()
    {
        if (_initialized)
        {
            return;
        }

        lock (_lock)
        {
            if (_initialized)
            {
                return;
            }

            NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, ResolveDll);
            _initialized = true;
        }
    }

    private static string AddLibraryExtension(string name) =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? $"{name}.dll" :
        RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? $"lib{name}.so" :
        RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? $"lib{name}.dylib" :
        throw new PlatformNotSupportedException();

    /// <summary>
    /// Add a directory to the process PATH so the OS loader finds transitive native
    /// dependencies when foundry_local is loaded. This is additive and process-scoped —
    /// unlike SetDefaultDllDirectories, it does not restrict the existing search order.
    /// </summary>
    private static void AddToNativeSearchPath(string directory)
    {
        var currentPath = Environment.GetEnvironmentVariable("PATH") ?? "";
        var dirs = currentPath.Split(Path.PathSeparator);

        if (!dirs.Contains(directory, StringComparer.OrdinalIgnoreCase))
        {
            Environment.SetEnvironmentVariable("PATH", directory + Path.PathSeparator + currentPath);
        }
    }

    /// <summary>
    /// Try to load from a redirect file written by the build when FoundryLocalNativeBinDir is set.
    /// This avoids copying any native binaries for local dev — they're loaded directly from the
    /// C++ build output directory.
    /// </summary>
    private static IntPtr TryLoadFromRedirect(string libraryName)
    {
        var cfgPath = Path.Combine(AppContext.BaseDirectory, RedirectFileName);
        if (!File.Exists(cfgPath))
        {
            return IntPtr.Zero;
        }

        var nativeBinDir = File.ReadAllText(cfgPath).Trim();
        if (string.IsNullOrEmpty(nativeBinDir) || !Directory.Exists(nativeBinDir))
        {
            return IntPtr.Zero;
        }

        var libraryPath = Path.Combine(nativeBinDir, AddLibraryExtension(libraryName));
        if (!File.Exists(libraryPath))
        {
            return IntPtr.Zero;
        }

        // Prepend to PATH so the OS loader finds transitive deps (ORT, azure-core, etc.)
        AddToNativeSearchPath(nativeBinDir);

        if (NativeLibrary.TryLoad(libraryPath, out var handle))
        {
            return handle;
        }

        return IntPtr.Zero;
    }

    private static IntPtr ResolveDll(string libraryName, System.Reflection.Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != NativeMethods.LibraryName)
        {
            return IntPtr.Zero;
        }

        // 0. Local dev redirect — load directly from C++ build output (no copy)
        var redirectResult = TryLoadFromRedirect(libraryName);
        if (redirectResult != IntPtr.Zero)
        {
            return redirectResult;
        }

        // 1. Check AppContext.BaseDirectory (co-located DLLs — flat publish / NuGet .targets copy)
        var libraryPath = Path.Combine(AppContext.BaseDirectory, AddLibraryExtension(libraryName));
        if (File.Exists(libraryPath))
        {
            AddToNativeSearchPath(AppContext.BaseDirectory);

            if (NativeLibrary.TryLoad(libraryPath, out var handle))
            {
                return handle;
            }
        }

        // 2. Check runtimes/<rid>/native/ (NuGet layout)
        var os = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win" :
                 RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? "linux" :
                 RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "osx" :
                 throw new PlatformNotSupportedException();

        var arch = RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant();
        var runtimePath = Path.Combine(AppContext.BaseDirectory, "runtimes", $"{os}-{arch}", "native");
        libraryPath = Path.Combine(runtimePath, AddLibraryExtension(libraryName));

        if (File.Exists(libraryPath))
        {
            AddToNativeSearchPath(runtimePath);

            if (NativeLibrary.TryLoad(libraryPath, out var handle))
            {
                return handle;
            }
        }

        // 3. Fall back to standard OS search path
        return IntPtr.Zero;
    }
}
