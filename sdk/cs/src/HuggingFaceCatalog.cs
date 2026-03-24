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
using System.Text.Json.Serialization;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

internal sealed class HuggingFaceCatalog : ICatalog, IDisposable
{
    private readonly Dictionary<string, ModelVariant> _modelIdToModelVariant = new();
    private readonly Dictionary<string, Model> _modelIdToModel = new();

    private readonly IModelLoadManager _modelLoadManager;
    private readonly ICoreInterop _coreInterop;
    private readonly ILogger _logger;
    private readonly AsyncLock _lock = new();
    private readonly string? _token;

    public string Name { get; init; }

    private HuggingFaceCatalog(IModelLoadManager modelLoadManager, ICoreInterop coreInterop, ILogger logger,
                               string? token)
    {
        _modelLoadManager = modelLoadManager;
        _coreInterop = coreInterop;
        _logger = logger;
        _token = token;

        Name = "HuggingFace";
    }

    internal static async Task<HuggingFaceCatalog> CreateAsync(IModelLoadManager modelManager,
                                                               ICoreInterop coreInterop,
                                                               ILogger logger,
                                                               string? token = null,
                                                               CancellationToken? ct = null)
    {
        var catalog = new HuggingFaceCatalog(modelManager, coreInterop, logger, token);
        await catalog.LoadRegistrationsAsync(ct).ConfigureAwait(false);
        return catalog;
    }

    public async Task<List<Model>> ListModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => ListModelsImplAsync(ct),
                                                     "Error listing HuggingFace models.", _logger)
                         .ConfigureAwait(false);
    }

    public async Task<Model?> GetModelAsync(string modelAlias, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelImplAsync(modelAlias, ct),
                                                     $"Error getting HuggingFace model '{modelAlias}'.", _logger)
                         .ConfigureAwait(false);
    }

    public async Task<Model> DownloadModelAsync(string modelUri, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => DownloadModelImplAsync(modelUri, ct),
            $"Error downloading HuggingFace model '{modelUri}'.", _logger)
                         .ConfigureAwait(false);
    }

    public async Task<Model> RegisterModelAsync(string modelIdentifier, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => RegisterModelImplAsync(modelIdentifier, ct),
                                                     $"Error registering HuggingFace model '{modelIdentifier}'.",
                                                     _logger)
                         .ConfigureAwait(false);
    }

    public async Task<ModelVariant?> GetModelVariantAsync(string modelId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelVariantImplAsync(modelId, ct),
                                                     $"Error getting HuggingFace model variant '{modelId}'.",
                                                     _logger)
                         .ConfigureAwait(false);
    }

    public async Task<List<ModelVariant>> GetCachedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetCachedModelsImplAsync(ct),
                                                     "Error getting cached HuggingFace models.", _logger)
                         .ConfigureAwait(false);
    }

    public async Task<List<ModelVariant>> GetLoadedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetLoadedModelsImplAsync(ct),
                                                     "Error getting loaded HuggingFace models.", _logger)
                         .ConfigureAwait(false);
    }

    private async Task<List<Model>> ListModelsImplAsync(CancellationToken? ct = null)
    {
        // HuggingFace catalog returns one entry per registration (each variant is individually referenceable)
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        return _modelIdToModel.Values.OrderBy(m => m.Id).ToList();
    }

    private async Task<Model?> GetModelImplAsync(string modelIdentifier, CancellationToken? ct = null)
    {
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);

        // Try direct Id lookup first
        if (_modelIdToModel.TryGetValue(modelIdentifier, out Model? model))
        {
            return model;
        }

        // Try alias lookup (returns first match)
        var aliasMatch = _modelIdToModel.Values.FirstOrDefault(m =>
            string.Equals(m.Alias, modelIdentifier, StringComparison.OrdinalIgnoreCase));
        if (aliasMatch != null)
        {
            return aliasMatch;
        }

        // Try URI-based lookup
        var normalizedUrl = NormalizeToHuggingFaceUrl(modelIdentifier);
        if (normalizedUrl != null)
        {
            var normalizedUrlWithSlash = normalizedUrl.TrimEnd('/') + "/";
            foreach (var variant in _modelIdToModelVariant.Values)
            {
                if (string.Equals(variant.Info.Uri, normalizedUrl, StringComparison.OrdinalIgnoreCase) ||
                    variant.Info.Uri.StartsWith(normalizedUrlWithSlash, StringComparison.OrdinalIgnoreCase))
                {
                    if (_modelIdToModel.TryGetValue(variant.Id, out Model? foundModel))
                    {
                        return foundModel;
                    }
                }
            }
        }

        return null;
    }

    private async Task<Model> RegisterModelImplAsync(string modelIdentifier, CancellationToken? ct = null)
    {
        // Validate it's a HuggingFace URL or org/repo format
        var normalizedUrl = NormalizeToHuggingFaceUrl(modelIdentifier);
        if (normalizedUrl == null)
        {
            throw new FoundryLocalException(
                $"'{modelIdentifier}' is not a valid HuggingFace URL or org/repo identifier.", _logger);
        }

        // Call Core to register the model (fetch metadata, generate inference_model.json, persist to
        // huggingface.modelinfo.json)
        var registerRequest = new CoreInteropRequest
        {
            Params = new Dictionary<string, string>
            {
                { "Model", modelIdentifier },
                { "Token", _token ?? "" }
            }
        };

        var result = await _coreInterop.ExecuteCommandAsync("register_model", registerRequest, ct)
                                       .ConfigureAwait(false);

        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error registering HuggingFace model '{modelIdentifier}': {result.Error}",
                                            _logger);
        }

        // Deserialize the returned ModelInfo
        var modelInfo = JsonSerializer.Deserialize(result.Data!, JsonSerializationContext.Default.ModelInfo);
        if (modelInfo == null)
        {
            throw new FoundryLocalException($"Failed to deserialize registered model metadata.", _logger);
        }

        // Add to internal dictionaries with lock
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        var variant = new ModelVariant(modelInfo, _modelLoadManager, _coreInterop, _logger, _token);
        _modelIdToModelVariant[modelInfo.Id] = variant;

        // Each registration is a distinct entry, keyed by Id
        var registeredModel = new Model(variant, _logger);
        _modelIdToModel[modelInfo.Id] = registeredModel;

        // Persist registrations to local file
        await SaveRegistrationsAsync(ct).ConfigureAwait(false);

        return registeredModel;
    }

    private async Task<Model> DownloadModelImplAsync(string modelUri, CancellationToken? ct)
    {
        // Validate it's a HuggingFace URL or org/repo format
        if (NormalizeToHuggingFaceUrl(modelUri) == null)
        {
            throw new FoundryLocalException(
                $"'{modelUri}' is not a valid HuggingFace URL or org/repo identifier.", _logger);
        }

        // Call Core's download_model command (same as existing Catalog)
        var downloadRequest = new CoreInteropRequest
        {
            Params = new Dictionary<string, string>
            {
                { "Model", modelUri },
                { "Token", _token ?? "" }
            }
        };

        var result = await _coreInterop.ExecuteCommandAsync("download_model", downloadRequest, ct)
                                       .ConfigureAwait(false);

        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error downloading model '{modelUri}': {result.Error}", _logger);
        }

        // The backend returns the org/model URI (e.g. "microsoft/Phi-3-mini") as result.Data
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        var expectedUri = $"https://huggingface.co/{result.Data}";
        var expectedUriWithSlash = expectedUri.TrimEnd('/') + "/";
        var matchingVariant = _modelIdToModelVariant.Values.FirstOrDefault(v =>
            string.Equals(v.Info.Uri, expectedUri, StringComparison.OrdinalIgnoreCase) ||
            v.Info.Uri.StartsWith(expectedUriWithSlash, StringComparison.OrdinalIgnoreCase) ||
            expectedUri.StartsWith(v.Info.Uri.TrimEnd('/') + "/", StringComparison.OrdinalIgnoreCase));

        if (matchingVariant != null)
        {
            if (_modelIdToModel.TryGetValue(matchingVariant.Id, out Model? hfModel))
            {
                return hfModel;
            }
        }

        throw new FoundryLocalException(
            $"Model '{modelUri}' was downloaded but could not be found in the catalog.", _logger);
    }

    private async Task<List<ModelVariant>> GetCachedModelsImplAsync(CancellationToken? ct = null)
    {
        var cachedModelIds = await Utils.GetCachedModelIdsAsync(_coreInterop, ct).ConfigureAwait(false);

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
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

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
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

    private async Task<ModelVariant?> GetModelVariantImplAsync(string modelId, CancellationToken? ct = null)
    {
        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        _modelIdToModelVariant.TryGetValue(modelId, out ModelVariant? modelVariant);
        return modelVariant;
    }

    private string GetRegistrationsPath()
    {
        var result = _coreInterop.ExecuteCommand("get_cache_directory");
        var cacheDir = result.Data?.Trim().Trim('"') ?? throw new InvalidOperationException("Failed to get cache directory from Core");
        return Path.Combine(cacheDir, "HuggingFace", "huggingface.modelinfo.json");
    }

    private async Task LoadRegistrationsAsync(CancellationToken? ct = null)
    {
        // Load persisted HuggingFace registrations from cache directory
        try
        {
            var registrationsPath = GetRegistrationsPath();

            if (!File.Exists(registrationsPath))
            {
                return; // No registrations yet
            }

            var registrationsJson = await File.ReadAllTextAsync(registrationsPath).ConfigureAwait(false);
            if (string.IsNullOrEmpty(registrationsJson))
            {
                return;
            }

            var models = JsonSerializer.Deserialize(registrationsJson, JsonSerializationContext.Default.ListModelInfo);
            if (models == null)
            {
                _logger.LogDebug("Failed to deserialize HuggingFace registrations from file");
                return;
            }

            using var disposable = await _lock.LockAsync().ConfigureAwait(false);

            foreach (var modelInfo in models)
            {
                var variant = new ModelVariant(modelInfo, _modelLoadManager, _coreInterop, _logger, _token);
                _modelIdToModelVariant[modelInfo.Id] = variant;
                _modelIdToModel[modelInfo.Id] = new Model(variant, _logger);
            }
        }
        catch (Exception ex)
        {
            _logger.LogWarning($"Exception loading HuggingFace registrations: {ex.Message}");
            // Continue anyway — empty catalog is valid
        }
    }

    private async Task SaveRegistrationsAsync(CancellationToken? ct = null)
    {
        // Save persisted HuggingFace registrations to cache directory
        try
        {
            var registrationsPath = GetRegistrationsPath();
            var registrationsDir = Path.GetDirectoryName(registrationsPath)!;

            // Ensure directory exists
            Directory.CreateDirectory(registrationsDir);

            // Snapshot registered models under lock, then do file I/O outside it
            List<ModelInfo> models;
            using (await _lock.LockAsync().ConfigureAwait(false))
            {
                models = _modelIdToModelVariant.Values
                    .Select(v => v.Info)
                    .Distinct()
                    .ToList();
            }

            // Serialize with pretty-printing (matching foundry.modelinfo.json style)
            var prettyOptions = new JsonSerializerOptions { WriteIndented = true, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };
            var prettyContext = new JsonSerializationContext(prettyOptions);
            var json = JsonSerializer.Serialize(models, prettyContext.ListModelInfo);
            await File.WriteAllTextAsync(registrationsPath, json).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogWarning($"Failed to save HuggingFace registrations: {ex.Message}");
            // Continue anyway — loss of persistence file is not critical
        }
    }

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
            // Strip /tree/{revision}/ from bare identifiers (e.g. "org/repo/tree/main/subpath")
            var parts = input.Split('/');
            if (parts.Length >= 4 &&
                parts[2].Equals("tree", StringComparison.OrdinalIgnoreCase))
            {
                var org = parts[0];
                var repo = parts[1];
                var subPath = parts.Length > 4 ? string.Join("/", parts.Skip(4)) : null;
                return subPath != null
                    ? $"{hfPrefix}{org}/{repo}/{subPath}"
                    : $"{hfPrefix}{org}/{repo}";
            }

            return hfPrefix + input;
        }

        return null;
    }

    public void Dispose()
    {
        _lock?.Dispose();
    }
}
