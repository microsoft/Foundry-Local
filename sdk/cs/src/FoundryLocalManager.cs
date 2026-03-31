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
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>The model catalog.</returns>
    /// <remarks>
    /// The catalog is populated on first use and returns models based on currently available execution providers.
    /// To ensure all hardware-accelerated models are listed, call <see cref="DownloadAndRegisterEpsAsync"/> first to
    /// register execution providers, then access the catalog.
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
    /// Discovers all available execution provider bootstrappers.
    /// Returns metadata about each EP including whether it is already registered.
    /// </summary>
    /// <returns>Array of EP bootstrapper info describing available EPs.</returns>
    public EpInfo[] DiscoverEps()
    {
        return Utils.CallWithExceptionHandling(DiscoverEpsImpl,
                                               "Error discovering execution providers.", _logger);
    }

    /// <summary>
    /// Downloads and registers execution providers. This is a blocking call that completes when all
    /// requested EPs have been processed.
    /// </summary>
    /// <param name="names">
    /// Optional subset of EP bootstrapper names to download (as returned by <see cref="DiscoverEps"/>).
    /// If null or empty, all discoverable EPs are downloaded.
    /// </param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Result describing which EPs succeeded and which failed.</returns>
    /// <remarks>
    /// Catalog and model requests use whatever EPs are currently registered and do not block on EP downloads.
    /// After downloading new EPs, re-fetch the model catalog to include models requiring the newly registered EPs.
    /// </remarks>
    public async Task<EpDownloadResult> DownloadAndRegisterEpsAsync(IEnumerable<string>? names = null,
                                                          CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => DownloadAndRegisterEpsImplAsync(names, null, ct),
                                                     "Error downloading execution providers.", _logger)
                                                    .ConfigureAwait(false);
    }

    /// <summary>
    /// Downloads and registers execution providers with per-EP progress reporting.
    /// </summary>
    public async Task DownloadAndRegisterEpsAsync(IEnumerable<string>? names,
                                                  Action<string, double> progressCallback,
                                                  CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => DownloadAndRegisterEpsImplAsync(names, progressCallback, ct),
                                              "Error downloading execution providers.", _logger)
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

    private EpInfo[] DiscoverEpsImpl()
    {
        var result = _coreInterop!.ExecuteCommand("discover_eps");
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error discovering execution providers: {result.Error}", _logger);
        }

        return JsonSerializer.Deserialize(result.Data!, JsonSerializationContext.Default.EpInfoArray)
            ?? Array.Empty<EpInfo>();
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

    private async Task<EpDownloadResult> DownloadAndRegisterEpsImplAsync(IEnumerable<string>? names = null,
                                                                Action<string, double>? progressCallback = null,
                                                                CancellationToken? ct = null)
    {
        using var disposable = await asyncLock.LockAsync().ConfigureAwait(false);

        CoreInteropRequest? input = null;
        if (names != null)
        {
            var namesList = string.Join(",", names);
            if (!string.IsNullOrEmpty(namesList))
            {
                input = new CoreInteropRequest
                {
                    Params = new Dictionary<string, string> { { "Names", namesList } }
                };
            }
        }

        ICoreInterop.Response result;

        if (progressCallback != null)
        {
            var callback = new ICoreInterop.CallbackFn(progressString =>
            {
                var sepIndex = progressString.IndexOf('|');
                if (sepIndex >= 0)
                {
                    var name = progressString[..sepIndex];
                    if (double.TryParse(progressString[(sepIndex + 1)..],
                                        System.Globalization.NumberStyles.Float,
                                        System.Globalization.CultureInfo.InvariantCulture,
                                        out var percent))
                    {
                        progressCallback(string.IsNullOrEmpty(name) ? "" : name, percent);
                    }
                }
            });

            result = await _coreInterop!.ExecuteCommandWithCallbackAsync("download_and_register_eps", input,
                                                                         callback, ct).ConfigureAwait(false);
        }
        else
        {
            result = await _coreInterop!.ExecuteCommandAsync("download_and_register_eps", input, ct).ConfigureAwait(false);
        }

        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error downloading execution providers: {result.Error}", _logger);
        }

        EpDownloadResult epResult;

        if (!string.IsNullOrEmpty(result.Data))
        {
            epResult = JsonSerializer.Deserialize(result.Data!, JsonSerializationContext.Default.EpDownloadResult)
                ?? throw new FoundryLocalException("Failed to deserialize EP download result.", _logger);
        }
        else
        {
            epResult = new EpDownloadResult { Success = true, Status = "Completed", RegisteredEps = [], FailedEps = [] };
        }

        // Invalidate the catalog cache if any EP was newly registered so the next access
        // re-fetches models with the updated set of available EPs.
        if ((epResult.Success || epResult.RegisteredEps.Length > 0) && _catalog != null)
        {
            _catalog.InvalidateCache();
        }

        return epResult;
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
