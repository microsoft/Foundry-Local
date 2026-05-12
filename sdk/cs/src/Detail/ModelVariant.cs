// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

internal class ModelVariant : IModel
{
    private readonly IModelLoadManager _modelLoadManager;
    private readonly ICoreInterop _coreInterop;
    private readonly ILogger _logger;

    public ModelInfo Info { get; } // expose the full info record

    // expose a few common properties directly
    public string Id => Info.Id;
    public string Alias => Info.Alias;
    public int Version { get; init; }  // parsed from Info.Version if possible, else 0

    public IReadOnlyList<IModel> Variants => [this];

    internal ModelVariant(ModelInfo modelInfo, IModelLoadManager modelLoadManager, ICoreInterop coreInterop,
                          ILogger logger)
    {
        Info = modelInfo;
        Version = modelInfo.Version;

        _modelLoadManager = modelLoadManager;
        _coreInterop = coreInterop;
        _logger = logger;

    }

    // simpler and always correct to check if loaded from the model load manager
    // this allows for multiple instances of ModelVariant to exist
    public async Task<bool> IsLoadedAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => IsLoadedImplAsync(ct),
                                                     "Error checking if model is loaded", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<bool> IsCachedAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => IsCachedImplAsync(ct),
                                                     "Error checking if model is cached", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<string> GetPathAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetPathImplAsync(ct),
                                                     "Error getting path for model", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task DownloadAsync(Action<float>? downloadProgress = null,
                                    CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => DownloadImplAsync(downloadProgress, ct),
                                              $"Error downloading model {Id}", _logger)
                                             .ConfigureAwait(false);
    }

    public async Task LoadAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => _modelLoadManager.LoadAsync(Id, ct),
                                              "Error loading model", _logger)
                                             .ConfigureAwait(false);
    }

    public async Task UnloadAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => _modelLoadManager.UnloadAsync(Id, ct),
                                              "Error unloading model", _logger)
                                             .ConfigureAwait(false);
    }

    public async Task RemoveFromCacheAsync(CancellationToken? ct = null)
    {
        await Utils.CallWithExceptionHandling(() => RemoveFromCacheImplAsync(ct),
                                              $"Error removing model {Id} from cache", _logger)
                                             .ConfigureAwait(false);
    }

    public async Task<OpenAIChatClient> GetChatClientAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetChatClientImplAsync(ct),
                                                     "Error getting chat client for model", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<OpenAIAudioClient> GetAudioClientAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetAudioClientImplAsync(ct),
                                                     "Error getting audio client for model", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<OpenAIEmbeddingClient> GetEmbeddingClientAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetEmbeddingClientImplAsync(ct),
                                                     "Error getting embedding client for model", _logger)
                                                    .ConfigureAwait(false);
    }

    private async Task<bool> IsLoadedImplAsync(CancellationToken? ct = null)
    {
        var loadedModels = await _modelLoadManager.ListLoadedModelsAsync(ct).ConfigureAwait(false);
        return loadedModels.Contains(Id);
    }

    private async Task<bool> IsCachedImplAsync(CancellationToken? ct = null)
    {
        var cachedModelIds = await Utils.GetCachedModelIdsAsync(_coreInterop, ct).ConfigureAwait(false);
        return cachedModelIds.Contains(Id);
    }

    private async Task<string> GetPathImplAsync(CancellationToken? ct = null)
    {
        var request = new CoreInteropRequest { Params = new Dictionary<string, string> { { "Model", Id } } };
        var result = await _coreInterop.ExecuteCommandAsync("get_model_path", request, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw Utils.FromNativeError("get_model_path", result.Error, ct, _logger,
                                        context: $"Error getting path for model {Id} (has it been downloaded?)");
        }

        var path = result.Data!;
        return path;
    }

    // Re-fire the user's progress callback every 5s when the native layer
    // goes quiet, so spinners/UIs don't look hung during slow blob reads.
    private static readonly TimeSpan DownloadHeartbeatInterval = TimeSpan.FromSeconds(5);

    private async Task DownloadImplAsync(Action<float>? downloadProgress = null,
                                         CancellationToken? ct = null)
    {
        var request = new CoreInteropRequest { Params = new() { { "Model", Id } } };
        ICoreInterop.Response? response;

        if (downloadProgress == null)
        {
            response = await _coreInterop.ExecuteCommandAsync("download_model", request, ct).ConfigureAwait(false);
        }
        else
        {
            float lastProgress = 0f;
            var lastUtc = DateTime.UtcNow;
            var sync = new object();

            var callback = new ICoreInterop.CallbackFn(s =>
            {
                if (float.TryParse(s, out var p))
                {
                    lock (sync) { lastProgress = p; lastUtc = DateTime.UtcNow; }
                    downloadProgress(p);
                }
            });

            using var hbCts = new CancellationTokenSource();
            var hb = Task.Run(async () =>
            {
                try
                {
                    while (!hbCts.IsCancellationRequested)
                    {
                        await Task.Delay(DownloadHeartbeatInterval, hbCts.Token).ConfigureAwait(false);
                        float p; TimeSpan idle;
                        lock (sync) { p = lastProgress; idle = DateTime.UtcNow - lastUtc; }
                        if (idle >= DownloadHeartbeatInterval)
                        {
                            _logger.LogDebug("Download heartbeat {ModelId}: {Pct:F1}% (idle {Idle:F0}s)",
                                             Id, p, idle.TotalSeconds);
                            try { downloadProgress(p); } catch { /* don't let a buggy callback abort the download */ }
                        }
                    }
                }
                catch (OperationCanceledException) { }
            }, hbCts.Token);

            try
            {
                response = await _coreInterop.ExecuteCommandWithCallbackAsync("download_model", request,
                                                                              callback, ct).ConfigureAwait(false);
            }
            finally
            {
                hbCts.Cancel();
                try { await hb.ConfigureAwait(false); } catch { }
            }
        }

        if (response.Error != null)
        {
            throw Utils.FromNativeError("download_model", response.Error, ct, _logger,
                                        context: $"Error downloading model {Id}");
        }
    }

    private async Task RemoveFromCacheImplAsync(CancellationToken? ct = null)
    {
        var request = new CoreInteropRequest { Params = new Dictionary<string, string> { { "Model", Id } } };

        var result = await _coreInterop.ExecuteCommandAsync("remove_cached_model", request, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw Utils.FromNativeError("remove_cached_model", result.Error, ct, _logger,
                                        context: $"Error removing model {Id} from cache");
        }
    }

    private async Task<OpenAIChatClient> GetChatClientImplAsync(CancellationToken? ct = null)
    {
        if (!await IsLoadedAsync(ct))
        {
            throw new FoundryLocalException($"Model {Id} is not loaded. Call LoadAsync first.");
        }

        return new OpenAIChatClient(Id);
    }

    private async Task<OpenAIAudioClient> GetAudioClientImplAsync(CancellationToken? ct = null)
    {
        if (!await IsLoadedAsync(ct))
        {
            throw new FoundryLocalException($"Model {Id} is not loaded. Call LoadAsync first.");
        }

        return new OpenAIAudioClient(Id);
    }

    private async Task<OpenAIEmbeddingClient> GetEmbeddingClientImplAsync(CancellationToken? ct = null)
    {
        if (!await IsLoadedAsync(ct))
        {
            throw new FoundryLocalException($"Model {Id} is not loaded. Call LoadAsync first.");
        }

        return new OpenAIEmbeddingClient(Id);
    }

    public void SelectVariant(IModel variant)
    {
        throw new FoundryLocalException(
            $"SelectVariant is not supported on a ModelVariant. " +
            $"Call Catalog.GetModelAsync(\"{Alias}\") to get an IModel with all variants available.");
    }
}
