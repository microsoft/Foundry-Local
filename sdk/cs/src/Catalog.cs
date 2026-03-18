// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

internal sealed class Catalog : ICatalog, IDisposable
{
    private readonly Dictionary<string, Model> _modelAliasToModel = new();
    private readonly Dictionary<string, ModelVariant> _modelIdToModelVariant = new();
    private DateTime _lastFetch;

    private readonly IModelLoadManager _modelLoadManager;
    private readonly ICoreInterop _coreInterop;
    private readonly ILogger _logger;
    private readonly AsyncLock _lock = new();

    public string Name { get; init; }

    private Catalog(IModelLoadManager modelLoadManager, ICoreInterop coreInterop, ILogger logger)
    {
        _modelLoadManager = modelLoadManager;
        _coreInterop = coreInterop;
        _logger = logger;

        _lastFetch = DateTime.MinValue;

        CoreInteropRequest? input = null;
        var response = coreInterop.ExecuteCommand("get_catalog_name", input);
        if (response.Error != null)
        {
            throw new FoundryLocalException($"Error getting catalog name: {response.Error}", _logger);
        }

        Name = response.Data!;
    }

    internal static async Task<Catalog> CreateAsync(IModelLoadManager modelManager, ICoreInterop coreInterop,
                                                    ILogger logger, CancellationToken? ct = null)
    {
        var catalog = new Catalog(modelManager, coreInterop, logger);
        await catalog.UpdateModels(ct).ConfigureAwait(false);
        return catalog;
    }

    public async Task<List<Model>> ListModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => ListModelsImplAsync(ct),
                                                     "Error listing models.", _logger).ConfigureAwait(false);
    }

    public async Task<List<ModelVariant>> GetCachedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetCachedModelsImplAsync(ct),
                                                     "Error getting cached models.", _logger).ConfigureAwait(false);
    }

    public async Task<List<ModelVariant>> GetLoadedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetLoadedModelsImplAsync(ct),
                                                     "Error getting loaded models.", _logger).ConfigureAwait(false);
    }

    public async Task<Model?> GetModelAsync(string modelAlias, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelImplAsync(modelAlias, ct),
                                                     $"Error getting model with alias '{modelAlias}'.", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<ModelVariant?> GetModelVariantAsync(string modelId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelVariantImplAsync(modelId, ct),
                                                     $"Error getting model variant with ID '{modelId}'.", _logger)
                                                    .ConfigureAwait(false);
    }

    private async Task<List<Model>> ListModelsImplAsync(CancellationToken? ct = null)
    {
        await UpdateModels(ct).ConfigureAwait(false);

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        return _modelAliasToModel.Values.OrderBy(m => m.Alias).ToList();
    }

    private async Task<List<ModelVariant>> GetCachedModelsImplAsync(CancellationToken? ct = null)
    {
        var cachedModelIds = await Utils.GetCachedModelIdsAsync(_coreInterop, ct).ConfigureAwait(false);

        List<ModelVariant> cachedModels = new();
        foreach (var modelId in cachedModelIds)
        {
            if (_modelIdToModelVariant.TryGetValue(modelId, out ModelVariant? modelVariant))
            {
                cachedModels.Add(modelVariant);
            }
        }

        return cachedModels;
    }

    private async Task<List<ModelVariant>> GetLoadedModelsImplAsync(CancellationToken? ct = null)
    {
        var loadedModelIds = await _modelLoadManager.ListLoadedModelsAsync(ct).ConfigureAwait(false);
        List<ModelVariant> loadedModels = new();

        foreach (var modelId in loadedModelIds)
        {
            if (_modelIdToModelVariant.TryGetValue(modelId, out ModelVariant? modelVariant))
            {
                loadedModels.Add(modelVariant);
            }
        }

        return loadedModels;
    }

    private async Task<Model?> GetModelImplAsync(string modelAlias, CancellationToken? ct = null)
    {
        await UpdateModels(ct).ConfigureAwait(false);

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        _modelAliasToModel.TryGetValue(modelAlias, out Model? model);

        return model;
    }

    private async Task<ModelVariant?> GetModelVariantImplAsync(string modelId, CancellationToken? ct = null)
    {
        await UpdateModels(ct).ConfigureAwait(false);

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        _modelIdToModelVariant.TryGetValue(modelId, out ModelVariant? modelVariant);
        return modelVariant;
    }

    private async Task UpdateModels(CancellationToken? ct)
    {
        // TODO: make this configurable
        if (DateTime.Now - _lastFetch < TimeSpan.FromHours(6))
        {
            return;
        }

        CoreInteropRequest? input = null;
        var result = await _coreInterop.ExecuteCommandAsync("get_model_list", input, ct).ConfigureAwait(false);

        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error getting models: {result.Error}", _logger);
        }

        var models = JsonSerializer.Deserialize(result.Data!, JsonSerializationContext.Default.ListModelInfo);
        if (models == null)
        {
            _logger.LogDebug($"ListModelInfo deserialization error in UpdateModels. Data: {result.Data}");
            throw new FoundryLocalException($"Failed to deserialize models from response.", _logger);
        }

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);

        // TODO: Do we need to clear this out, or can we just add new models?
        _modelAliasToModel.Clear();
        _modelIdToModelVariant.Clear();

        foreach (var modelInfo in models)
        {
            var variant = new ModelVariant(modelInfo, _modelLoadManager, _coreInterop, _logger);

            var existingModel = _modelAliasToModel.TryGetValue(modelInfo.Alias, out Model? value);
            if (!existingModel)
            {
                value = new Model(variant, _logger);
                _modelAliasToModel[modelInfo.Alias] = value;
            }
            else
            {
                value!.AddVariant(variant);
            }

            _modelIdToModelVariant[variant.Id] = variant;
        }

        _lastFetch = DateTime.Now;
    }

    public void Dispose()
    {
        _lock.Dispose();
    }
}
