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
///
/// We also explicitly preload the ORT and GenAI native libraries by absolute path
/// before foundry_local is loaded. This is critical on Linux and macOS: the OS
/// loader processes foundry_local's DT_NEEDED libonnxruntime.so / .dylib entry at
/// dlopen time, so if ORT isn't already resident the load fails before any C++
/// code runs. Once ORT is loaded by absolute path, the dynamic linker resolves
/// foundry_local's NEEDED entries against the already-loaded modules by SONAME
/// and never goes through filesystem search. ORT must be loaded before GenAI
/// because GenAI's own NEEDED entry references ORT.
///
/// On Windows foundry_local uses delay-load for onnxruntime.dll, so the same
/// guarantee comes for free — but the explicit preload is still useful as it
/// pins the exact ORT we want the process to bind to (avoids a stale ORT on
/// PATH winning).
///
/// Must be initialized before any P/Invoke calls to the native library.
/// </summary>
internal static class DllLoader
{
    private static bool _initialized;
    private static readonly object _lock = new();

    // Hold ORT and GenAI handles for the lifetime of the process. .NET's
    // NativeLibrary doesn't unload on collection, but keeping a reference makes
    // the preload intent unambiguous and protects against future API changes.
    private static IntPtr _ortHandle;
    private static IntPtr _genAiHandle;

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
            LogResolution($"redirect file ({RedirectFileName})", libraryPath);
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

        // 0. Local dev redirect — load directly from C++ build output (no copy).
        // The redirect file is written by the SDK csproj only when FoundryLocalNativeBinDir
        // is set (i.e. an explicit local-dev override). In CI / NuGet-consumed builds the
        // file should never exist; if one is found here it almost certainly indicates a
        // stale dev-build artifact and is worth flagging in the load log so the source of
        // any "wrong DLL loaded" surprise is obvious from the test output.
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

            // Preload ORT and GenAI from the same directory BEFORE foundry_local loads.
            // See class doc comment for why this is required on Linux/macOS.
            PreloadOrtIfPresent(AppContext.BaseDirectory);

            if (NativeLibrary.TryLoad(libraryPath, out var handle))
            {
                LogResolution("BaseDirectory", libraryPath);
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

            // Preload ORT and GenAI from the same NuGet runtimes dir BEFORE foundry_local loads.
            PreloadOrtIfPresent(runtimePath);

            if (NativeLibrary.TryLoad(libraryPath, out var handle))
            {
                LogResolution($"runtimes/{os}-{arch}/native", libraryPath);
                return handle;
            }
        }

        // 3. Fall back to standard OS search path
        LogResolution("OS search path (fallback)", AddLibraryExtension(libraryName));
        return IntPtr.Zero;
    }

    /// <summary>
    /// Emit a one-line marker to stderr (and the diagnostic trace) recording which strategy
    /// produced the loaded library and from where. Stderr is used so the line shows up in
    /// CI test-output capture even when no ILogger is wired in.
    /// </summary>
    private static void LogResolution(string strategy, string path)
    {
        var msg = $"[FoundryLocal.DllLoader] resolved foundry_local via {strategy}: {path}";
        Console.Error.WriteLine(msg);
        System.Diagnostics.Trace.WriteLine(msg);
    }

    /// <summary>
    /// Preload onnxruntime then onnxruntime-genai by absolute path from <paramref name="directory"/>
    /// before foundry_local is loaded.
    ///
    /// On Linux/macOS this is required: dlopen processes foundry_local's DT_NEEDED
    /// libonnxruntime.so.1 entry at load time, so if ORT isn't already resident the
    /// foundry_local load fails with "libonnxruntime.so.1: cannot open shared object file".
    /// Once we load ORT by absolute path, the dynamic linker registers it in the loaded
    /// module table by SONAME (e.g. "libonnxruntime.so.1") and resolves foundry_local's
    /// NEEDED entry against that — no filesystem search, no RPATH involved.
    ///
    /// On Windows foundry_local uses delay-load for onnxruntime.dll so the preload isn't
    /// strictly required; it's still useful because it pins which ORT the process binds
    /// to (avoids a stale ORT on PATH winning).
    ///
    /// Idempotent and best-effort: missing files are silently skipped (the subsequent
    /// foundry_local load attempt will surface a clearer error).
    /// </summary>
    private static void PreloadOrtIfPresent(string directory)
    {
        // Order matters: GenAI's NEEDED entry references ORT, so ORT must be loaded first.
        if (_ortHandle == IntPtr.Zero)
        {
            _ortHandle = TryPreload(directory, "onnxruntime");
        }

        if (_genAiHandle == IntPtr.Zero)
        {
            _genAiHandle = TryPreload(directory, "onnxruntime-genai");
        }
    }

    private static IntPtr TryPreload(string directory, string libraryName)
    {
        var path = Path.Combine(directory, AddLibraryExtension(libraryName));
        if (!File.Exists(path))
        {
            return IntPtr.Zero;
        }

        if (NativeLibrary.TryLoad(path, out var handle))
        {
            var msg = $"[FoundryLocal.DllLoader] preloaded {libraryName}: {path}";
            Console.Error.WriteLine(msg);
            System.Diagnostics.Trace.WriteLine(msg);
            return handle;
        }

        var failMsg = $"[FoundryLocal.DllLoader] failed to preload {libraryName} from {path}";
        Console.Error.WriteLine(failMsg);
        System.Diagnostics.Trace.WriteLine(failMsg);
        return IntPtr.Zero;
    }
}