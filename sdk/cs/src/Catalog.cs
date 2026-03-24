// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System;
using System.Collections.Generic;
using System.Linq;
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

    public async Task<Model> DownloadModelAsync(string modelUri, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => DownloadModelImplAsync(modelUri, ct),
                                                     $"Error downloading model '{modelUri}'.", _logger)
                                                    .ConfigureAwait(false);
    }

    public Task<Model> RegisterModelAsync(string modelIdentifier, CancellationToken? ct = null)
    {
        return Task.FromException<Model>(new NotSupportedException(
            "RegisterModelAsync is only available on HuggingFace catalogs. Use AddCatalogAsync(\"https://huggingface.co\") to create a HuggingFace catalog."));
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
        var hfUrl = NormalizeToHuggingFaceUrl(modelAlias);
        if (hfUrl != null)
        {
            // Force a fresh catalog refresh for HuggingFace lookups
            _lastFetch = DateTime.MinValue;
            await UpdateModels(ct).ConfigureAwait(false);

            using var disposable = await _lock.LockAsync().ConfigureAwait(false);
            var matchingVariant = _modelIdToModelVariant.Values.FirstOrDefault(v =>
                string.Equals(v.Info.Uri, hfUrl, StringComparison.OrdinalIgnoreCase));

            if (matchingVariant != null)
            {
                _modelAliasToModel.TryGetValue(matchingVariant.Alias, out Model? hfModel);
                return hfModel;
            }

            return null;
        }

        await UpdateModels(ct).ConfigureAwait(false);

        using var d = await _lock.LockAsync().ConfigureAwait(false);
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

    private async Task<Model> DownloadModelImplAsync(string modelUri, CancellationToken? ct)
    {
        // Validate that this is a HuggingFace identifier
        if (NormalizeToHuggingFaceUrl(modelUri) == null)
        {
            throw new FoundryLocalException(
                $"'{modelUri}' is not a valid HuggingFace URL or org/repo identifier.", _logger);
        }

        // Send the original URI to Core — it handles full URLs with /tree/revision/
        // and raw org/repo/subdir strings. Do NOT send the normalized form, as Core's
        // URL parser expects /tree/revision/ when the https:// prefix is present.
        var downloadRequest = new CoreInteropRequest
        {
            Params = new Dictionary<string, string> { { "Model", modelUri } }
        };

        var result = await _coreInterop.ExecuteCommandAsync("download_model", downloadRequest, ct)
                                       .ConfigureAwait(false);

        if (result.Error != null)
        {
            throw new FoundryLocalException(
                $"Error downloading model '{modelUri}': {result.Error}", _logger);
        }

        // Force a catalog refresh to pick up the newly downloaded model
        _lastFetch = DateTime.MinValue;
        await UpdateModels(ct).ConfigureAwait(false);

        // The backend returns the org/model URI (e.g. "microsoft/Phi-3-mini") as result.Data
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        var expectedUri = $"https://huggingface.co/{result.Data}";
        var matchingVariant = _modelIdToModelVariant.Values.FirstOrDefault(v =>
            string.Equals(v.Info.Uri, expectedUri, StringComparison.OrdinalIgnoreCase));

        if (matchingVariant != null)
        {
            _modelAliasToModel.TryGetValue(matchingVariant.Alias, out Model? hfModel);
            return hfModel!;
        }

        throw new FoundryLocalException(
            $"Model '{modelUri}' was downloaded but could not be found in the catalog.", _logger);
    }

    /// <summary>
    /// Normalizes a model identifier to a canonical HuggingFace URL, or returns null if it's a plain alias.
    /// Strips /tree/{revision}/ from full browser URLs so the result matches the stored Info.Uri format.
    /// Handles:
    ///   - "https://huggingface.co/org/repo/tree/main/sub" -> "https://huggingface.co/org/repo/sub"
    ///   - "https://huggingface.co/org/repo" -> returned as-is
    ///   - "org/repo[/sub]" -> "https://huggingface.co/org/repo[/sub]"
    ///   - "phi-3-mini" (plain alias) -> null
    /// </summary>
    private static string? NormalizeToHuggingFaceUrl(string input)
    {
        const string hfPrefix = "https://huggingface.co/";

        if (input.StartsWith(hfPrefix, StringComparison.OrdinalIgnoreCase))
        {
            // Strip /tree/{revision}/ to match the canonical form stored by Core
            var path = input[hfPrefix.Length..];
            var parts = path.Split('/');
            if (parts.Length >= 4 &&
                parts[2].Equals("tree", StringComparison.OrdinalIgnoreCase))
            {
                // parts[0]=org, parts[1]=repo, parts[2]="tree", parts[3]=revision, parts[4..]=subpath
                var org = parts[0];
                var repo = parts[1];
                var subPath = parts.Length > 4 ? string.Join("/", parts.Skip(4)) : null;
                return subPath != null
                    ? $"{hfPrefix}{org}/{repo}/{subPath}"
                    : $"{hfPrefix}{org}/{repo}";
            }

            return input;
        }

        if (input.Contains('/') && !input.StartsWith("azureml://", StringComparison.OrdinalIgnoreCase))
        {
            return hfPrefix + input;
        }

        return null;
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
