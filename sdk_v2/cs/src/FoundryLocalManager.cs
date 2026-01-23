// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------
namespace Microsoft.AI.Foundry.Local;

using System;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

public class FoundryLocalManager : IDisposable
{
    private static FoundryLocalManager? instance;
    private static readonly AsyncLock asyncLock = new();

    internal static readonly string AssemblyVersion =
        typeof(FoundryLocalManager).Assembly.GetName().Version?.ToString() ?? "unknown";

    private readonly Configuration _config;
    private CoreInterop _coreInterop = default!;
    private Catalog _catalog = default!;
    private ModelLoadManager _modelManager = default!;
    private readonly AsyncLock _lock = new();
    private bool _disposed;
    private readonly ILogger _logger;

    internal Configuration Configuration => _config;
    internal ILogger Logger => _logger;
    internal ICoreInterop CoreInterop => _coreInterop!; // always valid once the instance is created

    public static bool IsInitialized => instance != null;
    public static FoundryLocalManager Instance => instance ??
        throw new FoundryLocalException("FoundryLocalManager has not been created. Call CreateAsync first.");

    /// <summary>
    /// Bound Urls if the web service has been started. Null otherwise.
    /// See <see cref="StartWebServiceAsync"/>.
    /// </summary>
    public string[]? Urls { get; private set; }

    /// <summary>
    /// Create the <see cref="FoundryLocalManager"/> singleton instance.
    /// </summary>
    /// <param name="configuration">Configuration to use.</param>
    /// <param name="logger">Application logger to use.
    /// Use Microsoft.Extensions.Logging.NullLogger.Instance if you wish to ignore log output from the SDK.
    /// </param>
    /// <param name="ct">Optional cancellation token for the initialization.</param>
    /// <returns>Task creating the instance.</returns>
    /// <exception cref="FoundryLocalException"></exception>
    public static async Task CreateAsync(Configuration configuration, ILogger logger,
                                         CancellationToken? ct = null)
    {
        using var disposable = await asyncLock.LockAsync().ConfigureAwait(false);

        if (instance != null)
        {
            // throw as we're not going to use the provided configuration in case it differs from the original.
            throw new FoundryLocalException("FoundryLocalManager has already been created.", logger);
        }

        FoundryLocalManager? manager = null;
        try
        {
            // use a local variable to ensure fully initialized before assigning to static instance.
            manager = new FoundryLocalManager(configuration, logger);
            await manager.InitializeAsync(ct).ConfigureAwait(false);

            // there is no previous as we only get here if instance is null.
            // ownership is transferred to the static instance.
#pragma warning disable IDISP003 // Dispose previous before re-assigning
            instance = manager;
            manager = null;
#pragma warning restore IDISP003
        }
        catch (Exception ex)
        {
            manager?.Dispose();

            if (ex is FoundryLocalException or OperationCanceledException)
            {
                throw;
            }

            // log and throw as FoundryLocalException
            throw new FoundryLocalException("Error during initialization.", ex, logger);
        }
    }

    /// <summary>
    /// Get the model catalog instance.
    /// </summary>
    /// <param name="ct">Optional canellation token.</param>
    /// <returns>The model catalog.</returns>
    /// <remarks>
    /// The catalog is populated on first use.
    /// If you are using a WinML build this will trigger a one-off execution provider download if not already done.
    /// It is recommended to call <see cref="EnsureEpsDownloadedAsync"/> first to separate out the two steps.
    /// </remarks>
    public async Task<ICatalog> GetCatalogAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetCatalogImplAsync(ct),
                                                     "Error getting Catalog.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Start the optional web service. This will provide an OpenAI-compatible REST endpoint that supports
    ///   /v1/chat_completions
    ///   /v1/models to list downloaded models
    ///   /v1/models/{model_id} to get model details
    ///
    /// <see cref="Urls"/> is populated with the actual bound Urls after startup.
    /// </summary>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Task starting the web service.</returns>
    public async Task StartWebServiceAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => StartWebServiceImplAsync(ct),
                                              "Error starting web service.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Stops the web service if started.
    /// </summary>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Task stopping the web service.</returns>
    public async Task StopWebServiceAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => StopWebServiceImplAsync(ct),
                                              "Error stopping web service.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Ensure execution providers are downloaded and registered.
    /// Only relevant when using WinML.
    ///
    /// Execution provider download can be time consuming due to the size of the packages.
    /// Once downloaded, EPs are not re-downloaded unless a new version is available, so this method will be fast
    /// on subsequent calls.
    /// </summary>
    /// <param name="ct">Optional cancellation token.</param>
    public async Task EnsureEpsDownloadedAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => EnsureEpsDownloadedImplAsync(ct),
                                              "Error ensuring execution providers downloaded.", _logger)
                                             .ConfigureAwait(false);
    }

    private FoundryLocalManager(Configuration configuration, ILogger logger)
    {
        _config = configuration ?? throw new ArgumentNullException(nameof(configuration));
        _logger = logger;
    }

    private async Task InitializeAsync(CancellationToken? ct = null)
    {
        _config.Validate();
        _coreInterop = new CoreInterop(_config, _logger);

#pragma warning disable IDISP003 // Dispose previous before re-assigning. Always null when this is called.
        _modelManager = new ModelLoadManager(_config.Web?.ExternalUrl, _coreInterop, _logger);
#pragma warning restore IDISP003

        if (_config.ModelCacheDir != null)
        {
            CoreInteropRequest? input = null;
            var result = await _coreInterop!.ExecuteCommandAsync("get_cache_directory", input, ct)
                                            .ConfigureAwait(false);
            if (result.Error != null)
            {
                throw new FoundryLocalException($"Error getting current model cache directory: {result.Error}",
                                                _logger);
            }

            var curCacheDir = result.Data!;
            if (curCacheDir != _config.ModelCacheDir)
            {
                var request = new CoreInteropRequest
                {
                    Params = new Dictionary<string, string> { { "Directory", _config.ModelCacheDir } }
                };

                result = await _coreInterop!.ExecuteCommandAsync("set_cache_directory", request, ct)
                                            .ConfigureAwait(false);
                if (result.Error != null)
                {
                    throw new FoundryLocalException(
                        $"Error setting model cache directory to '{_config.ModelCacheDir}': {result.Error}", _logger);
                }
            }
        }

        return;
    }

    private async Task<ICatalog> GetCatalogImplAsync(CancellationToken? ct = null)
    {
        // create on first use
        if (_catalog == null)
        {
            using var disposable = await _lock.LockAsync().ConfigureAwait(false);
            if (_catalog == null)
            {
                _catalog = await Catalog.CreateAsync(_modelManager!, _coreInterop!, _logger, ct).ConfigureAwait(false);
            }
        }

        return _catalog;
    }

    private async Task StartWebServiceImplAsync(CancellationToken? ct = null)
    {
        if (_config?.Web?.Urls == null)
        {
            throw new FoundryLocalException("Web service configuration was not provided.", _logger);
        }

        using var disposable = await asyncLock.LockAsync().ConfigureAwait(false);

        CoreInteropRequest? input = null;
        var result = await _coreInterop!.ExecuteCommandAsync("start_service", input, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error starting web service: {result.Error}", _logger);
        }

        var typeInfo = JsonSerializationContext.Default.StringArray;
        var boundUrls = JsonSerializer.Deserialize(result.Data!, typeInfo);
        if (boundUrls == null || boundUrls.Length == 0)
        {
            throw new FoundryLocalException("Failed to get bound URLs from web service start response.", _logger);
        }

        Urls = boundUrls;
    }

    private async Task StopWebServiceImplAsync(CancellationToken? ct = null)
    {
        if (_config?.Web?.Urls == null)
        {
            throw new FoundryLocalException("Web service configuration was not provided.", _logger);
        }

        using var disposable = await asyncLock.LockAsync().ConfigureAwait(false);

        CoreInteropRequest? input = null;
        var result = await _coreInterop!.ExecuteCommandAsync("stop_service", input, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error stopping web service: {result.Error}", _logger);
        }

        // Should we clear these even if there's an error response?
        // Service is probably in a bad state or was not running.
        Urls = null;
    }

    private async Task EnsureEpsDownloadedImplAsync(CancellationToken? ct = null)
    {

        using var disposable = await asyncLock.LockAsync().ConfigureAwait(false);

        CoreInteropRequest? input = null;
        var result = await _coreInterop!.ExecuteCommandAsync("ensure_eps_downloaded", input, ct);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error ensuring execution providers downloaded: {result.Error}", _logger);
        }
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                if (Urls != null)
                {
                    // best effort stop
                    try
                    {
                        StopWebServiceImplAsync().GetAwaiter().GetResult();
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning(ex, "Error stopping web service during Dispose.");
                    }
                }

                _catalog?.Dispose();
                _modelManager?.Dispose();
                _lock.Dispose();
            }

            _disposed = true;
        }
    }

    public void Dispose()
    {
        // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
        Dispose(disposing: true);
        GC.SuppressFinalize(this);
    }
}
