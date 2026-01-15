// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Diagnostics;
using System.Runtime.InteropServices;

using Microsoft.Extensions.Logging;

using static Microsoft.AI.Foundry.Local.Detail.ICoreInterop;

internal partial class CoreInterop : ICoreInterop
{
    // TODO: Android and iOS may need special handling. See ORT C# NativeMethods.shared.cs
    internal const string LibraryName = "Microsoft.AI.Foundry.Local.Core";
    private readonly ILogger _logger;

    private static string AddLibraryExtension(string name) =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? $"{name}.dll" :
        RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? $"{name}.so" :
        RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? $"{name}.dylib" :
        throw new PlatformNotSupportedException();

    private static IntPtr genaiLibHandle = IntPtr.Zero;
    private static IntPtr ortLibHandle = IntPtr.Zero;

    // we need to manually load ORT and ORT GenAI dlls on Windows to ensure
    // a) we're using the libraries we think we are
    // b) that dependencies are resolved correctly as the dlls may not be in the default load path.
    // it's a 'Try' as we can't do anything else if it fails as the dlls may be available somewhere else.
    private static void LoadOrtDllsIfInSameDir(string path)
    {
        var genaiLibName = AddLibraryExtension("onnxruntime-genai");
        var ortLibName = AddLibraryExtension("onnxruntime");
        var genaiPath = Path.Combine(path, genaiLibName);
        var ortPath = Path.Combine(path, ortLibName);

        // need to load ORT first as the winml GenAI library redirects and tries to load a winml onnxruntime.dll,
        // which will not have the EPs we expect/require. if/when we don't bundle our own onnxruntime.dll we need to
        // revisit this.
        var loadedOrt = NativeLibrary.TryLoad(ortPath, out ortLibHandle);
        var loadedGenAI = NativeLibrary.TryLoad(genaiPath, out genaiLibHandle);

#if DEBUG
        Console.WriteLine($"Loaded ORT:{loadedOrt} handle={ortLibHandle}");
        Console.WriteLine($"Loaded GenAI: {loadedGenAI} handle={genaiLibHandle}");
#endif
    }

    static CoreInterop()
    {
        NativeLibrary.SetDllImportResolver(typeof(CoreInterop).Assembly, (libraryName, assembly, searchPath) =>
        {
            if (libraryName == LibraryName)
            {
#if DEBUG
                Console.WriteLine($"Resolving {libraryName}. BaseDirectory: {AppContext.BaseDirectory}");
#endif
                var isWindows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);

                // check if this build is platform specific. in that case all files are flattened in the one directory
                // and there's no need to look in runtimes/<os>-<arch>/native.
                // e.g. `dotnet publish -r win-x64` copies all the dependencies into the publish output folder.
                var libraryPath = Path.Combine(AppContext.BaseDirectory, AddLibraryExtension(LibraryName));
                if (File.Exists(libraryPath))
                {
                    if (NativeLibrary.TryLoad(libraryPath, out var handle))
                    {
#if DEBUG
                        Console.WriteLine($"Loaded native library from: {libraryPath}");
#endif
                        if (isWindows)
                        {
                            LoadOrtDllsIfInSameDir(AppContext.BaseDirectory);
                        }

                        return handle;
                    }
                }

                // TODO: figure out what is required on Android and iOS
                // The nuget has an AAR and xcframework respectively so we need to determine what files are where
                // after a build.
                var os = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win" :
                         RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? "linux" :
                         RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "osx" :
                         throw new PlatformNotSupportedException();

                var arch = RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant();
                var runtimePath = Path.Combine(AppContext.BaseDirectory, "runtimes", $"{os}-{arch}", "native");
                libraryPath = Path.Combine(runtimePath, AddLibraryExtension(LibraryName));

#if DEBUG
                Console.WriteLine($"Looking for native library at: {libraryPath}");
#endif
                if (File.Exists(libraryPath))
                {
                    if (NativeLibrary.TryLoad(libraryPath, out var handle))
                    {
#if DEBUG
                        Console.WriteLine($"Loaded native library from: {libraryPath}");
#endif
                        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
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

    internal CoreInterop(Configuration config, ILogger logger)
    {

        _logger = logger ?? throw new ArgumentNullException(nameof(logger));

        var request = new CoreInteropRequest { Params = config.AsDictionary() };
        var response = ExecuteCommand("initialize", request);

        if (response.Error != null)
        {
            throw new FoundryLocalException($"Error initializing Foundry.Local.Core library: {response.Error}");
        }
        else
        {
            _logger.LogInformation("Foundry.Local.Core initialized successfully: {Response}", response.Data);
        }
    }

    // For testing. Skips the 'initialize' command so assumes this has been done previously.
    internal CoreInterop(ILogger logger)
    {
        _logger = logger ?? throw new ArgumentNullException(nameof(logger));
    }


    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private unsafe delegate void ExecuteCommandDelegate(RequestBuffer* req, ResponseBuffer* resp);

    // Import the function from the AOT-compiled library
    [LibraryImport(LibraryName, EntryPoint = "execute_command")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreExecuteCommand(RequestBuffer* request, ResponseBuffer* response);

    [LibraryImport(LibraryName, EntryPoint = "execute_command_with_callback")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    private static unsafe partial void CoreExecuteCommandWithCallback(RequestBuffer* nativeRequest,
                                                                      ResponseBuffer* nativeResponse,
                                                                      nint callbackPtr, // NativeCallbackFn pointer
                                                                      nint userData);

    // helper to capture exceptions in callbacks
    internal class CallbackHelper
    {
        public CallbackFn Callback { get; }
        public Exception? Exception { get; set; } // keep the first only. most likely it will be the same issue in all
        public CallbackHelper(CallbackFn callback)
        {
            Callback = callback ?? throw new ArgumentNullException(nameof(callback));
        }
    }

    private static void HandleCallback(nint data, int length, nint callbackHelper)
    {
        var callbackData = string.Empty;
        CallbackHelper? helper = null;

        try
        {
            if (data != IntPtr.Zero && length > 0)
            {
                var managedData = new byte[length];
                Marshal.Copy(data, managedData, 0, length);
                callbackData = System.Text.Encoding.UTF8.GetString(managedData);
            }

            Debug.Assert(callbackHelper != IntPtr.Zero, "Callback helper pointer is required.");

            helper = (CallbackHelper)GCHandle.FromIntPtr(callbackHelper).Target!;
            helper.Callback.Invoke(callbackData);
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            FoundryLocalManager.Instance.Logger.LogError(ex, $"Error in callback. Callback data: {callbackData}");
            if (helper != null && helper.Exception == null)
            {
                helper.Exception = ex;
            }
        }
    }

    private static readonly NativeCallbackFn handleCallbackDelegate = HandleCallback;


    public Response ExecuteCommandImpl(string commandName, string? commandInput,
                                       CallbackFn? callback = null)
    {
        try
        {
            byte[] commandBytes = System.Text.Encoding.UTF8.GetBytes(commandName);
            // Allocate unmanaged memory for the command bytes
            IntPtr commandPtr = Marshal.AllocHGlobal(commandBytes.Length);
            Marshal.Copy(commandBytes, 0, commandPtr, commandBytes.Length);

            byte[]? inputBytes = null;
            IntPtr? inputPtr = null;

            if (commandInput != null)
            {
                inputBytes = System.Text.Encoding.UTF8.GetBytes(commandInput);
                inputPtr = Marshal.AllocHGlobal(inputBytes.Length);
                Marshal.Copy(inputBytes, 0, inputPtr.Value, inputBytes.Length);
            }

            // Prepare request
            var request = new RequestBuffer
            {
                Command = commandPtr,
                CommandLength = commandBytes.Length,
                Data = inputPtr ?? IntPtr.Zero,
                DataLength = inputBytes?.Length ?? 0
            };

            ResponseBuffer response = default;

            if (callback != null)
            {
                // NOTE: This assumes the command will NOT return until complete, so the lifetime of the
                //       objects involved in the callback is limited to the duration of the call to
                //       CoreExecuteCommandWithCallback.

                var helper = new CallbackHelper(callback);

                var funcPtr = Marshal.GetFunctionPointerForDelegate(handleCallbackDelegate);
                var helperHandle = GCHandle.Alloc(helper);
                var helperPtr = GCHandle.ToIntPtr(helperHandle);

                unsafe
                {
                    CoreExecuteCommandWithCallback(&request, &response, funcPtr, helperPtr);
                }

                helperHandle.Free();

                if (helper.Exception != null)
                {
                    throw new FoundryLocalException("Exception in callback handler. See InnerException for details",
                                                    helper.Exception);
                }
            }
            else
            {
                // Pin request/response on the stack
                unsafe
                {
                    CoreExecuteCommand(&request, &response);
                }
            }

            Response result = new();

            // Marshal response. Will have either Data or Error populated. Not both.
            if (response.Data != IntPtr.Zero && response.DataLength > 0)
            {
                byte[] managedResponse = new byte[response.DataLength];
                Marshal.Copy(response.Data, managedResponse, 0, response.DataLength);
                result.Data = System.Text.Encoding.UTF8.GetString(managedResponse);
                _logger.LogDebug($"Command: {commandName} succeeded.");
            }

            if (response.Error != IntPtr.Zero && response.ErrorLength > 0)
            {
                result.Error = Marshal.PtrToStringUTF8(response.Error, response.ErrorLength)!;
                _logger.LogDebug($"Input:{commandInput ?? "null"}");
                _logger.LogDebug($"Command: {commandName} Error: {result.Error}");
            }

            // TODO: Validate this works. C# specific. Attempting to avoid calling free_response to do this
            Marshal.FreeHGlobal(response.Data);
            Marshal.FreeHGlobal(response.Error);

            Marshal.FreeHGlobal(commandPtr);
            if (commandInput != null)
            {
                Marshal.FreeHGlobal(inputPtr!.Value);
            }

            return result;
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            var msg = $"Error executing command '{commandName}' with input {commandInput ?? "null"}";
            throw new FoundryLocalException(msg, ex, _logger);
        }
    }

    public Response ExecuteCommand(string commandName, CoreInteropRequest? commandInput = null)
    {
        var commandInputJson = commandInput?.ToJson();
        return ExecuteCommandImpl(commandName, commandInputJson);
    }

    public Response ExecuteCommandWithCallback(string commandName, CoreInteropRequest? commandInput,
                                               CallbackFn callback)
    {
        var commandInputJson = commandInput?.ToJson();
        return ExecuteCommandImpl(commandName, commandInputJson, callback);
    }

    public Task<Response> ExecuteCommandAsync(string commandName, CoreInteropRequest? commandInput = null,
                                              CancellationToken? cancellationToken = null)
    {
        var ct = cancellationToken ?? CancellationToken.None;
        return Task.Run(() => ExecuteCommand(commandName, commandInput), ct);
    }

    public Task<Response> ExecuteCommandWithCallbackAsync(string commandName, CoreInteropRequest? commandInput,
                                                          CallbackFn callback,
                                                          CancellationToken? cancellationToken = null)
    {
        var ct = cancellationToken ?? CancellationToken.None;
        return Task.Run(() => ExecuteCommandWithCallback(commandName, commandInput, callback), ct);
    }

}
