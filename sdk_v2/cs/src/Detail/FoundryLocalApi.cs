// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// High-level managed API mirroring the C++ foundry_local class hierarchy.
// Native handles are internal implementation details; public API returns domain classes.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.AI.Foundry.Local.Detail.Interop;

// Suppress IDisposableAnalyzers warnings for the bindings layer:
// - IDISP015: Item cache pattern is intentional (ModelList holds non-owning Model references)
// - IDISP023: Item.~Item finalizer accesses Api which is a static, always available
#pragma warning disable IDISP015
#pragma warning disable IDISP023

namespace Microsoft.AI.Foundry.Local.Detail.Native
{
    // ===================================================================
    // Internal API bootstrap (hidden from consumers)
    // ===================================================================

    internal static class Api
    {
        internal static FlApi Root;
        internal static FlItemApi Item;
        internal static FlInferenceApi Inference;
        internal static FlConfigurationApi Config;
        internal static FlCatalogApi Catalog;
        internal static FlModelApi Model;
        private static bool _initialized;

        internal static void EnsureInitialized()
        {
            if (_initialized)
            {
                return;
            }

            var apiPtr = NativeMethods.FoundryLocalGetApi(NativeMethods.ApiVersion);
            if (apiPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException(
                    $"FoundryLocalGetApi returned null for version {NativeMethods.ApiVersion}.");
            }

            Root = Marshal.PtrToStructure<FlApi>(apiPtr);

            var p = Root.GetItemApi();
            if (p != IntPtr.Zero)
            {
                Item = Marshal.PtrToStructure<FlItemApi>(p);
            }

            p = Root.GetInferenceApi();
            if (p != IntPtr.Zero)
            {
                Inference = Marshal.PtrToStructure<FlInferenceApi>(p);
            }

            p = Root.GetConfigurationApi();
            if (p != IntPtr.Zero)
            {
                Config = Marshal.PtrToStructure<FlConfigurationApi>(p);
            }

            p = Root.GetCatalogApi();
            if (p != IntPtr.Zero)
            {
                Catalog = Marshal.PtrToStructure<FlCatalogApi>(p);
            }

            p = Root.GetModelApi();
            if (p != IntPtr.Zero)
            {
                Model = Marshal.PtrToStructure<FlModelApi>(p);
            }

            _initialized = true;
        }

        internal static void CheckStatus(IntPtr status)
        {
            if (status == IntPtr.Zero)
            {
                return;
            }

            var code = Root.StatusGetErrorCode(status);
            var msgPtr = Root.StatusGetErrorMessage(status);
            var msg = msgPtr == IntPtr.Zero ? "Unknown error" : Marshal.PtrToStringUTF8(msgPtr) ?? "Unknown error";
            Root.StatusRelease(status);

            if (code == FlErrorCode.OperationCancelled)
            {
                throw new OperationCanceledException(msg);
            }

            throw new Microsoft.AI.Foundry.Local.FoundryLocalException(msg);
        }

        internal static string? Utf8(IntPtr ptr) =>
            ptr == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(ptr);
    }

    // ===================================================================
    // Static entry point
    // ===================================================================

    public static class FoundryLocal
    {
        public static string GetVersionString()
        {
            var ptr = NativeMethods.FoundryLocalGetVersionString();
            return ptr == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
        }
    }

    // ===================================================================
    // Configuration — fluent builder, owns flConfiguration*
    // ===================================================================

    public sealed class Configuration : IDisposable
    {
        internal IntPtr Ptr { get; private set; }
        private bool _disposed;

        public Configuration(string appName)
        {
            Api.EnsureInitialized();
            var status = Api.Config.Create(appName, out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
        }

        public Configuration SetAppDataDir(string dir)
        {
            Api.CheckStatus(Api.Config.SetAppDataDir(Ptr, dir));
            return this;
        }

        public Configuration SetLogsDir(string dir)
        {
            Api.CheckStatus(Api.Config.SetLogsDir(Ptr, dir));
            return this;
        }

        public Configuration SetModelCacheDir(string dir)
        {
            Api.CheckStatus(Api.Config.SetModelCacheDir(Ptr, dir));
            return this;
        }

        public Configuration SetRuntimeLibraryPath(string dir)
        {
            Api.CheckStatus(Api.Config.SetRuntimeLibraryPath(Ptr, dir));
            return this;
        }

        public Configuration SetDefaultLogLevel(FlLogLevel level)
        {
            Api.CheckStatus(Api.Config.SetDefaultLogLevel(Ptr, level));
            return this;
        }

        public Configuration AddCatalogUrl(string url, string? filterOverride = null)
        {
            Api.CheckStatus(Api.Config.AddCatalogUrl(Ptr, url, filterOverride));
            return this;
        }

        public Configuration AddWebServiceEndpoint(string url)
        {
            Api.CheckStatus(Api.Config.AddWebServiceEndpoint(Ptr, url));
            return this;
        }

        public Configuration SetAdditionalOptions(IntPtr options)
        {
            Api.CheckStatus(Api.Config.SetAdditionalOptions(Ptr, options));
            return this;
        }

        public Configuration SetExternalServiceUrl(string url)
        {
            Api.CheckStatus(Api.Config.SetExternalServiceUrl(Ptr, url));
            return this;
        }

        public Configuration SetCatalogRegion(string region)
        {
            Api.CheckStatus(Api.Config.SetCatalogRegion(Ptr, region));
            return this;
        }

        public void Dispose()
        {
            if (!_disposed && Ptr != IntPtr.Zero)
            {
                Api.Config.Release(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    // ===================================================================
    // Manager — owns flManager*, created from Configuration
    // ===================================================================

    public sealed class Manager : IDisposable
    {
        internal IntPtr Ptr { get; private set; }
        private bool _disposed;

        public Manager(Configuration config)
        {
            Api.EnsureInitialized();
            var status = Api.Root.ManagerCreate(config.Ptr, out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
        }

        public Catalog GetCatalog()
        {
            var status = Api.Root.ManagerGetCatalog(Ptr, out var catalogPtr);
            Api.CheckStatus(status);
            return new Catalog(catalogPtr);
        }

        public void StartService()
        {
            Api.CheckStatus(Api.Root.ManagerWebServiceStart(Ptr));
        }

        public void StopService()
        {
            Api.CheckStatus(Api.Root.ManagerWebServiceStop(Ptr));
        }

        public string[] GetServiceUrls()
        {
            var status = Api.Root.ManagerWebServiceUrls(Ptr, out var urlsPtr, out var count);
            Api.CheckStatus(status);

            var numUrls = (int)(ulong)count;
            var urls = new string[numUrls];
            for (int i = 0; i < numUrls; i++)
            {
                var strPtr = Marshal.ReadIntPtr(urlsPtr, i * IntPtr.Size);
                urls[i] = Marshal.PtrToStringUTF8(strPtr) ?? string.Empty;
            }

            return urls;
        }

        public EpInfo[] GetDiscoverableEps()
        {
            var status = Api.Root.ManagerGetDiscoverableEps(Ptr, out var namesPtr, out var isRegisteredPtr, out var count);
            Api.CheckStatus(status);

            var numEps = (int)(ulong)count;
            var result = new EpInfo[numEps];

            if (numEps > 0)
            {
                unsafe
                {
                    var names = (IntPtr*)namesPtr;
                    var registered = (int*)isRegisteredPtr;

                    for (int i = 0; i < numEps; i++)
                    {
                        result[i] = new EpInfo
                        {
                            Name = Marshal.PtrToStringUTF8(names[i]) ?? string.Empty,
                            IsRegistered = registered[i] != 0,
                        };
                    }
                }
            }

            return result;
        }

        public void DownloadAndRegisterEps(string[]? epNames, FlEpProgressCallback? callback)
        {
            IntPtr namesArray = IntPtr.Zero;
            UIntPtr nameCount = UIntPtr.Zero;
            GCHandle[]? handles = null;

            try
            {
                if (epNames != null && epNames.Length > 0)
                {
                    handles = new GCHandle[epNames.Length];
                    var ptrs = new IntPtr[epNames.Length];

                    for (int i = 0; i < epNames.Length; i++)
                    {
                        var bytes = System.Text.Encoding.UTF8.GetBytes(epNames[i] + '\0');
                        handles[i] = GCHandle.Alloc(bytes, GCHandleType.Pinned);
                        ptrs[i] = handles[i].AddrOfPinnedObject();
                    }

                    var ptrHandle = GCHandle.Alloc(ptrs, GCHandleType.Pinned);
                    namesArray = ptrHandle.AddrOfPinnedObject();
                    nameCount = (UIntPtr)epNames.Length;

                    var status = Api.Root.ManagerDownloadAndRegisterEps(Ptr, namesArray, nameCount, callback, IntPtr.Zero);
                    ptrHandle.Free();
                    Api.CheckStatus(status);
                }
                else
                {
                    var status = Api.Root.ManagerDownloadAndRegisterEps(Ptr, IntPtr.Zero, UIntPtr.Zero, callback, IntPtr.Zero);
                    Api.CheckStatus(status);
                }
            }
            finally
            {
                if (handles != null)
                {
                    foreach (var h in handles)
                    {
                        if (h.IsAllocated)
                        {
                            h.Free();
                        }
                    }
                }
            }
        }

        public bool IsEpDownloadInProgress()
        {
            return Api.Root.ManagerIsEpDownloadInProgress(Ptr);
        }

        public void Shutdown()
        {
            Api.CheckStatus(Api.Root.ManagerShutdown(Ptr));
        }

        public bool IsShutdownRequested()
        {
            return Api.Root.ManagerIsShutdownRequested(Ptr);
        }

        public void Dispose()
        {
            if (!_disposed && Ptr != IntPtr.Zero)
            {
                Api.Root.ManagerRelease(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    // ===================================================================
    // Catalog — non-owning (lifetime tied to Manager)
    // ===================================================================

    public sealed class Catalog
    {
        internal IntPtr Ptr { get; }

        internal Catalog(IntPtr ptr) => Ptr = ptr;

        public string GetName()
        {
            var status = Api.Catalog.GetName(Ptr, out var namePtr);
            Api.CheckStatus(status);
            return Api.Utf8(namePtr) ?? string.Empty;
        }

        public ModelList GetModels()
        {
            var status = Api.Catalog.GetModels(Ptr, out var ptr);
            Api.CheckStatus(status);
            return new ModelList(ptr);
        }

        public Model? GetModel(string alias)
        {
            var status = Api.Catalog.GetModel(Ptr, alias, out var ptr);
            Api.CheckStatus(status);
            return ptr == IntPtr.Zero ? null : new Model(ptr);
        }

        public Model? GetModelVariant(string modelId)
        {
            var status = Api.Catalog.GetModelVariant(Ptr, modelId, out var ptr);
            Api.CheckStatus(status);
            return ptr == IntPtr.Zero ? null : new Model(ptr);
        }

        public Model GetLatestVersion(Model model)
        {
            var status = Api.Catalog.GetLatestVersion(Ptr, model.Ptr, out var ptr);
            Api.CheckStatus(status);

            if (ptr == IntPtr.Zero)
            {
                // Defensive: native should already have returned INVALID_ARGUMENT (which
                // CheckStatus throws above). Reaching here means the input IModel was not
                // produced by this catalog — user error, since IModel instances should
                // only come from Catalog APIs.
                throw new FoundryLocalException(
                    "GetLatestVersion returned no model. The IModel argument was not produced by this catalog.");
            }

            return new Model(ptr);
        }

        public ModelList GetCachedModels()
        {
            var status = Api.Catalog.GetCachedModels(Ptr, out var ptr);
            Api.CheckStatus(status);
            return new ModelList(ptr);
        }

        public ModelList GetLoadedModels()
        {
            var status = Api.Catalog.GetLoadedModels(Ptr, out var ptr);
            Api.CheckStatus(status);
            return new ModelList(ptr);
        }
    }

    // ===================================================================
    // ModelInfo — non-owning read-only view (lifetime tied to Model)
    // ===================================================================

    public sealed class ModelInfo
    {
        private readonly IntPtr _ptr;

        internal ModelInfo(IntPtr ptr) => _ptr = ptr;

        // Core identity (always present — never null)
        public string Id => Api.Utf8(Api.Model.InfoGetId(_ptr))!;
        public string Name => Api.Utf8(Api.Model.InfoGetName(_ptr))!;
        public int Version => Api.Model.InfoGetVersion(_ptr);
        public string Alias => Api.Utf8(Api.Model.InfoGetAlias(_ptr))!;
        public string? Uri => Api.Utf8(Api.Model.InfoGetUri(_ptr));
        public FlDeviceType DeviceType => Api.Model.InfoGetDeviceType(_ptr);
        public string? ExecutionProvider => Api.Utf8(Api.Model.InfoGetExecutionProvider(_ptr));
        public string? Task => Api.Utf8(Api.Model.InfoGetTask(_ptr));

        // Generic property accessors
        public string? GetStringProperty(string key) =>
            Api.Utf8(Api.Model.InfoGetStringProperty(_ptr, key));

        public long GetIntProperty(string key, long defaultValue = -1) =>
            Api.Model.InfoGetIntProperty(_ptr, key, defaultValue);

        // Typed convenience properties
        public string? DisplayName => GetStringProperty(ModelProperties.DisplayName);
        public string? ModelType => GetStringProperty(ModelProperties.ModelType);
        public string? Publisher => GetStringProperty(ModelProperties.Publisher);
        public string? License => GetStringProperty(ModelProperties.License);
        public string? LicenseDescription => GetStringProperty(ModelProperties.LicenseDescription);
        public string? ModelProvider => GetStringProperty(ModelProperties.ModelProvider);
        public string? MinFlVersion => GetStringProperty(ModelProperties.MinFlVersion);
        public string? ParentUri => GetStringProperty(ModelProperties.ParentUri);
        public string? ToolCallStart => GetStringProperty(ModelProperties.ToolCallStart);
        public string? ToolCallEnd => GetStringProperty(ModelProperties.ToolCallEnd);

        public long SupportsToolCalling => GetIntProperty(ModelProperties.SupportsToolCalling, -1);
        public long FilesizeMb => GetIntProperty(ModelProperties.FileSizeMb, -1);
        public long MaxOutputTokens => GetIntProperty(ModelProperties.MaxOutputTokens, -1);
        public long CreatedAtUnix => GetIntProperty(ModelProperties.CreatedAtUnix, 0);
        public long IsTestModel => GetIntProperty(ModelProperties.IsTestModel, 0);
    }

    // ===================================================================
    // Model — non-owning (lifetime tied to ModelList/Catalog)
    // ===================================================================

    public sealed class Model
    {
        internal IntPtr Ptr { get; }

        internal Model(IntPtr ptr) => Ptr = ptr;

        public ModelInfo GetInfo()
        {
            var status = Api.Model.GetInfo(Ptr, out var infoPtr);
            Api.CheckStatus(status);
            return new ModelInfo(infoPtr);
        }

        public bool IsCached
        {
            get
            {
                var status = Api.Model.IsCached(Ptr, out var cached);
                Api.CheckStatus(status);
                return cached != 0;
            }
        }

        public bool IsLoaded
        {
            get
            {
                var status = Api.Model.IsLoaded(Ptr, out var loaded);
                Api.CheckStatus(status);
                return loaded != 0;
            }
        }

        public string? GetPath()
        {
            var status = Api.Model.GetPath(Ptr, out var pathPtr);
            Api.CheckStatus(status);
            return Api.Utf8(pathPtr);
        }

        internal InputOutputInfo GetInputOutputInfo()
        {
            var status = Api.Model.GetInputOutputInfo(Ptr,
                out var inputsPtr, out var numInputs, out var outputsPtr, out var numOutputs);
            Api.CheckStatus(status);

            var inputs = new List<Item>();
            for (int i = 0; i < (int)(ulong)numInputs; i++)
            {
                var itemPtr = Marshal.ReadIntPtr(inputsPtr, i * IntPtr.Size);
                inputs.Add(new Item(itemPtr, ownsHandle: false));
            }

            var outputs = new List<Item>();
            for (int i = 0; i < (int)(ulong)numOutputs; i++)
            {
                var itemPtr = Marshal.ReadIntPtr(outputsPtr, i * IntPtr.Size);
                outputs.Add(new Item(itemPtr, ownsHandle: false));
            }

            return new InputOutputInfo(inputs, outputs);
        }

        public void Download(Func<float, int>? progress = null)
        {
            FlProgressCallback? nativeCallback = null;
            if (progress != null)
            {
                nativeCallback = (value, _) => progress(value);
            }

            var status = Api.Model.Download(Ptr, nativeCallback, IntPtr.Zero);
            Api.CheckStatus(status);
        }

        public void Load()
        {
            Api.CheckStatus(Api.Model.Load(Ptr));
        }

        public void Unload()
        {
            Api.CheckStatus(Api.Model.Unload(Ptr));
        }

        public void RemoveFromCache()
        {
            Api.CheckStatus(Api.Model.RemoveFromCache(Ptr));
        }

        public ModelList GetVariants()
        {
            var status = Api.Model.GetVariants(Ptr, out var ptr);
            Api.CheckStatus(status);
            return new ModelList(ptr);
        }

        public void SelectVariant(Model variant)
        {
            Api.CheckStatus(Api.Model.SelectVariant(Ptr, variant.Ptr));
        }
    }

    // ===================================================================
    // ModelList — owns flModelList*, disposes it
    // ===================================================================

    public sealed class ModelList : IDisposable
    {
        private IntPtr _ptr;
        private bool _disposed;
        private readonly List<Model> _models;

        internal ModelList(IntPtr ptr)
        {
            _ptr = ptr;
            var count = (int)(ulong)Api.Root.ModelListSize(ptr);
            _models = new List<Model>(count);
            for (int i = 0; i < count; i++)
            {
                _models.Add(new Model(Api.Root.ModelListGetAt(ptr, (UIntPtr)i)));
            }
        }

        public IReadOnlyList<Model> Models => _models;

        public void Dispose()
        {
            if (!_disposed && _ptr != IntPtr.Zero)
            {
                Api.Root.ModelListRelease(_ptr);
                _ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    // ===================================================================
    // Content structs (pure data — mirrors C versioned structs)
    // ===================================================================

    internal sealed class TextContent
    {
        public string Text { get; }

        public FlTextItemType Type { get; }

        internal TextContent(string text, FlTextItemType type)
        {
            Text = text;
            Type = type;
        }
    }

    internal sealed class TensorContent
    {
        public FlTensorDataType DataType { get; }
        public IntPtr Data { get; }
        public long[] Shape { get; }

        internal TensorContent(FlTensorDataType dataType, IntPtr data, long[] shape)
        {
            DataType = dataType;
            Data = data;
            Shape = shape;
        }
    }

    internal sealed class ImageContent
    {
        public IntPtr Data { get; }
        public int DataSize { get; }
        public string? Format { get; }
        public string? Uri { get; }

        internal ImageContent(IntPtr data, int dataSize, string? format, string? uri)
        {
            Data = data;
            DataSize = dataSize;
            Format = format;
            Uri = uri;
        }
    }

    internal sealed class AudioContent
    {
        public IntPtr Data { get; }
        public int DataSize { get; }
        public string? Format { get; }
        public string? Uri { get; }

        internal AudioContent(IntPtr data, int dataSize, string? format, string? uri)
        {
            Data = data;
            DataSize = dataSize;
            Format = format;
            Uri = uri;
        }
    }

    internal sealed class MessageContent
    {
        public FlMessageRole Role { get; }
        public IReadOnlyList<IntPtr> ContentItems { get; }
        public string? Name { get; }

        internal MessageContent(FlMessageRole role, IReadOnlyList<IntPtr> contentItems, string? name)
        {
            Role = role;
            ContentItems = contentItems;
            Name = name;
        }
    }

    internal sealed class ToolCallContent
    {
        public string? CallId { get; }
        public string? Name { get; }
        public string? Arguments { get; }

        internal ToolCallContent(string? callId, string? name, string? arguments)
        {
            CallId = callId;
            Name = name;
            Arguments = arguments;
        }
    }

    internal sealed class ToolResultContent
    {
        public string? CallId { get; }
        public string? Result { get; }

        internal ToolResultContent(string? callId, string? result)
        {
            CallId = callId;
            Result = result;
        }
    }

    // ===================================================================
    // InputOutputInfo
    // ===================================================================

    internal sealed class InputOutputInfo
    {
        public IReadOnlyList<Item> Inputs { get; }
        public IReadOnlyList<Item> Outputs { get; }

        internal InputOutputInfo(List<Item> inputs, List<Item> outputs)
        {
            Inputs = inputs;
            Outputs = outputs;
        }
    }

    // ===================================================================
    // Item — base class for all item types. Can be owning or non-owning.
    // Typed subclasses (TextItem, MessageItem, etc.) add specialized constructors.
    // ===================================================================

    // Subclasses are sealed and have no extra native resources — virtual dispose not needed.
#pragma warning disable IDISP025
    internal class Item : IDisposable
#pragma warning restore IDISP025
    {
        internal IntPtr Ptr { get; private set; }
        private bool _ownsHandle;
        private bool _disposed;

        internal Item(IntPtr ptr, bool ownsHandle)
        {
            Ptr = ptr;
            _ownsHandle = ownsHandle;
        }

        protected Item(FlItemType type)
        {
            Api.EnsureInitialized();
            var status = Api.Item.Create(type, out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
            _ownsHandle = true;
        }

        public FlItemType ItemType => Api.Item.GetType(Ptr);

        public TextContent GetText()
        {
            var data = new FlTextData { Version = NativeMethods.ApiVersion };
            Api.CheckStatus(Api.Item.GetText(Ptr, out data));
            return new TextContent(Api.Utf8(data.Text) ?? string.Empty, data.Type);
        }

        public TensorContent GetTensor()
        {
            var status = Api.Item.GetTensor(Ptr, out var tensor);
            Api.CheckStatus(status);

            var rankInt = (int)(ulong)tensor.Rank;
            var shape = new long[rankInt];
            Marshal.Copy(tensor.Shape, shape, 0, rankInt);
            return new TensorContent(tensor.DataType, tensor.Data, shape);
        }

        public ImageContent GetImage()
        {
            var status = Api.Item.GetImage(Ptr, out var image);
            Api.CheckStatus(status);
            return new ImageContent(image.Data, (int)(ulong)image.DataSize,
                Api.Utf8(image.Format), Api.Utf8(image.Uri));
        }

        public AudioContent GetAudio()
        {
            var status = Api.Item.GetAudio(Ptr, out var audio);
            Api.CheckStatus(status);
            return new AudioContent(audio.Data, (int)(ulong)audio.DataSize,
                Api.Utf8(audio.Format), Api.Utf8(audio.Uri));
        }

        public MessageContent GetMessage()
        {
            var status = Api.Item.GetMessage(Ptr, out var message);
            Api.CheckStatus(status);

            var count = (int)(ulong)message.ContentItemsCount;
            var parts = new IntPtr[count];
            if (count > 0 && message.ContentItems != IntPtr.Zero)
            {
                Marshal.Copy(message.ContentItems, parts, 0, count);
            }

            return new MessageContent(message.Role, parts, Api.Utf8(message.Name));
        }

        public ToolCallContent GetToolCall()
        {
            var status = Api.Item.GetToolCall(Ptr, out var toolCall);
            Api.CheckStatus(status);
            return new ToolCallContent(Api.Utf8(toolCall.CallId), Api.Utf8(toolCall.Name), Api.Utf8(toolCall.Arguments));
        }

        public ToolResultContent GetToolResult()
        {
            var status = Api.Item.GetToolResult(Ptr, out var toolResult);
            Api.CheckStatus(status);
            return new ToolResultContent(Api.Utf8(toolResult.CallId), Api.Utf8(toolResult.Result));
        }

        /// <summary>
        /// Transfer ownership to the caller (e.g., before adding to a Request).
        /// After this call the item will no longer release the native handle.
        /// </summary>
        internal IntPtr ReleaseOwnership()
        {
            _ownsHandle = false;
            return Ptr;
        }

        public void Dispose()
        {
            if (!_disposed && _ownsHandle && Ptr != IntPtr.Zero)
            {
                Api.Item.Release(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }

            GC.SuppressFinalize(this);
        }
    }

    internal sealed class TextItem : Item
    {
        public TextItem(string text) : this(text, FlTextItemType.Default)
        {
        }

        public TextItem(string text, FlTextItemType type) : base(FlItemType.Text)
        {
            var textNative = Marshal.StringToCoTaskMemUTF8(text);

            try
            {
                var data = new FlTextData
                {
                    Version = NativeMethods.ApiVersion,
                    Text = textNative,
                    Type = type,
                };
                Api.CheckStatus(Api.Item.SetText(Ptr, ref data));
            }
            finally
            {
                Marshal.FreeCoTaskMem(textNative);
            }
        }
    }

    internal sealed class ImageItem : Item
    {
        /// <summary>Create from raw bytes. format is e.g. "png".</summary>
        public ImageItem(string format, IntPtr data, int dataSize) : base(FlItemType.Image)
        {
            var formatNative = Marshal.StringToCoTaskMemUTF8(format);
            try
            {
                var imageData = new FlImageData
                {
                    Version = NativeMethods.ApiVersion,
                    Data = data,
                    DataSize = (UIntPtr)dataSize,
                    Format = formatNative,
                    Uri = IntPtr.Zero,
                };
                Api.CheckStatus(Api.Item.SetImage(Ptr, ref imageData));
            }
            finally
            {
                Marshal.FreeCoTaskMem(formatNative);
            }
        }

        /// <summary>Create from a URI (file path, URL, etc.).</summary>
        public ImageItem(string uri, string? format = null) : base(FlItemType.Image)
        {
            var uriNative = Marshal.StringToCoTaskMemUTF8(uri);
            var formatNative = format != null ? Marshal.StringToCoTaskMemUTF8(format) : IntPtr.Zero;
            try
            {
                var imageData = new FlImageData
                {
                    Version = NativeMethods.ApiVersion,
                    Data = IntPtr.Zero,
                    DataSize = UIntPtr.Zero,
                    Format = formatNative,
                    Uri = uriNative,
                };
                Api.CheckStatus(Api.Item.SetImage(Ptr, ref imageData));
            }
            finally
            {
                Marshal.FreeCoTaskMem(uriNative);
                if (formatNative != IntPtr.Zero) Marshal.FreeCoTaskMem(formatNative);
            }
        }
    }

    internal sealed class AudioItem : Item
    {
        /// <summary>Create from a URI (file path, URL, etc.).</summary>
        public AudioItem(string uri, string? format = null) : base(FlItemType.Audio)
        {
            var uriNative = Marshal.StringToCoTaskMemUTF8(uri);
            var formatNative = format != null ? Marshal.StringToCoTaskMemUTF8(format) : IntPtr.Zero;
            try
            {
                var audioData = new FlAudioData
                {
                    Version = NativeMethods.ApiVersion,
                    Data = IntPtr.Zero,
                    MutableData = IntPtr.Zero,
                    DataSize = UIntPtr.Zero,
                    Format = formatNative,
                    Uri = uriNative,
                    Deleter = IntPtr.Zero,
                    DeleterUserData = IntPtr.Zero,
                };
                Api.CheckStatus(Api.Item.SetAudio(Ptr, ref audioData));
            }
            finally
            {
                Marshal.FreeCoTaskMem(uriNative);
                if (formatNative != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(formatNative);
                }
            }
        }
    }

    internal sealed class ToolCallItem : Item
    {
        public ToolCallItem(string callId, string name, string arguments)
            : base(FlItemType.ToolCall)
        {
            var callIdNative = Marshal.StringToCoTaskMemUTF8(callId);
            var nameNative = Marshal.StringToCoTaskMemUTF8(name);
            var argsNative = Marshal.StringToCoTaskMemUTF8(arguments);
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
    }

    internal sealed class ToolResultItem : Item
    {
        public ToolResultItem(string callId, string result) : base(FlItemType.ToolResult)
        {
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
    }

    // ===================================================================
    // Request — owns flRequest*
    // ===================================================================

    internal sealed class Request : IDisposable
    {
        internal IntPtr Ptr { get; private set; }
        private bool _disposed;

        public Request()
        {
            Api.EnsureInitialized();
            var status = Api.Inference.RequestCreate(out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
        }

        /// <summary>Add an item to the request. Transfers ownership — do not use the item after this.</summary>
        public Request AddItem(Item item)
        {
            var nativePtr = item.ReleaseOwnership();
            Api.CheckStatus(Api.Inference.RequestAddItem(Ptr, nativePtr, true));
            return this;
        }

        public int ItemCount => (int)(ulong)Api.Inference.RequestGetItemCount(Ptr);

        public Item GetItem(int index)
        {
            var status = Api.Inference.RequestGetItem(Ptr, (UIntPtr)index, out var itemPtr);
            Api.CheckStatus(status);
            return new Item(itemPtr, ownsHandle: false);
        }

        public Request SetOptions(IntPtr options)
        {
            Api.CheckStatus(Api.Inference.RequestSetOptions(Ptr, options));
            return this;
        }

        public void Cancel()
        {
            Api.CheckStatus(Api.Inference.RequestCancel(Ptr));
        }

        public void Dispose()
        {
            if (!_disposed && Ptr != IntPtr.Zero)
            {
                Api.Inference.RequestRelease(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    // ===================================================================
    // Response — owns flResponse*
    // ===================================================================

    internal sealed class Response : IDisposable
    {
        internal IntPtr Ptr { get; private set; }
        private bool _disposed;

        public Response()
        {
            Api.EnsureInitialized();
            var status = Api.Inference.ResponseCreate(out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
        }

        internal Response(IntPtr ptr)
        {
            Ptr = ptr;
        }

        public int ItemCount => (int)(ulong)Api.Inference.ResponseGetItemCount(Ptr);

        public Item GetItem(int index)
        {
            var status = Api.Inference.ResponseGetItem(Ptr, (UIntPtr)index, out var itemPtr);
            Api.CheckStatus(status);
            return new Item(itemPtr, ownsHandle: false);
        }

        public FlFinishReason FinishReason => Api.Inference.ResponseGetFinishReason(Ptr);

        public FlUsage GetUsage()
        {
            var status = Api.Inference.ResponseGetUsage(Ptr, out var usage);
            Api.CheckStatus(status);
            return usage;
        }

        public void Dispose()
        {
            if (!_disposed && Ptr != IntPtr.Zero)
            {
                Api.Inference.ResponseRelease(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    // ===================================================================
    // Session — owns flSession*, created from a loaded Model
    // ===================================================================

    public sealed class Session : IDisposable
    {
        internal IntPtr Ptr { get; private set; }
        private bool _disposed;
        private FlStreamingCallback? _callbackRef;  // prevent GC

        public Session(Model model)
        {
            Api.EnsureInitialized();
            var status = Api.Inference.SessionCreate(model.Ptr, out var ptr);
            Api.CheckStatus(status);
            Ptr = ptr;
        }

        /// <summary>Add a tool definition to the session. The session copies the data.</summary>
        public Session AddToolDefinition(string name, string description, string jsonSchema)
        {
            var nameNative = Marshal.StringToCoTaskMemUTF8(name);
            var descNative = Marshal.StringToCoTaskMemUTF8(description);
            var schemaNative = Marshal.StringToCoTaskMemUTF8(jsonSchema);
            try
            {
                var toolDef = new FlToolDefinition
                {
                    Version = NativeMethods.ApiVersion,
                    Name = nameNative,
                    Description = descNative,
                    JsonSchema = schemaNative,
                };
                Api.CheckStatus(Api.Inference.SessionAddToolDefinition(Ptr, ref toolDef));
            }
            finally
            {
                Marshal.FreeCoTaskMem(nameNative);
                Marshal.FreeCoTaskMem(descNative);
                Marshal.FreeCoTaskMem(schemaNative);
            }

            return this;
        }

        /// <summary>Set a streaming callback. Pass null to unset.</summary>
        public Session SetStreamingCallback(FlStreamingCallback? callback)
        {
            _callbackRef = callback;
            Api.CheckStatus(Api.Inference.SessionSetStreamingCallback(Ptr, callback, IntPtr.Zero));
            return this;
        }

        /// <summary>Set session-level inference options. Applies to all subsequent ProcessRequest calls.</summary>
        public Session SetOptions(IntPtr options)
        {
            Api.CheckStatus(Api.Inference.SessionSetOptions(Ptr, options));
            return this;
        }

        /// <summary>
        /// Process a request. Returns a new Response pointer. Caller owns it.
        /// </summary>
        internal IntPtr ProcessRequest(IntPtr requestPtr)
        {
            IntPtr responsePtr = IntPtr.Zero;
            Api.CheckStatus(Api.Inference.SessionProcessRequest(Ptr, requestPtr, ref responsePtr));
            return responsePtr;
        }

        /// <summary>Get the number of completed turns in the session.</summary>
        public ulong TurnCount => (ulong)Api.Inference.SessionGetTurnCount(Ptr);

        /// <summary>
        /// Undo the last <paramref name="count"/> turns: rewinds the generator and removes
        /// the turns' messages from history. If all turns are undone, the cached generator is destroyed.
        /// </summary>
        public void UndoTurns(ulong count)
        {
            Api.CheckStatus(Api.Inference.SessionUndoTurns(Ptr, checked((UIntPtr)count)));
        }

        public void Dispose()
        {
            if (!_disposed && Ptr != IntPtr.Zero)
            {
                _callbackRef = null;
                Api.Inference.SessionRelease(Ptr);
                Ptr = IntPtr.Zero;
                _disposed = true;
            }
        }
    }
}
