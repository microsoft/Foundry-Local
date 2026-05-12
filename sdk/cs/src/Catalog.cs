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

    public async Task<List<IModel>> ListModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => ListModelsImplAsync(ct),
                                                     "Error listing models.", _logger).ConfigureAwait(false);
    }

    public async Task<List<IModel>> ListModelsAsync(string catalogRegistryName, CancellationToken? ct = null)
    {
        if (string.IsNullOrWhiteSpace(catalogRegistryName))
        {
            throw new ArgumentException("Catalog registry name must be a non-empty string.", nameof(catalogRegistryName));
        }
        var all = await ListModelsAsync(ct).ConfigureAwait(false);
        // Match Model itself or any variant: ModelInfo merges variants per alias,
        // so the variant check is needed for public+private alias collisions.
        return all.Where(m => m.Info.IsFromCatalogRegistry(catalogRegistryName) ||
                              m.Variants.Any(v => v.Info.IsFromCatalogRegistry(catalogRegistryName)))
                  .ToList();
    }

    public async Task<List<IModel>> GetCachedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetCachedModelsImplAsync(ct),
                                                     "Error getting cached models.", _logger).ConfigureAwait(false);
    }

    public async Task<List<IModel>> GetLoadedModelsAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetLoadedModelsImplAsync(ct),
                                                     "Error getting loaded models.", _logger).ConfigureAwait(false);
    }

    public async Task<IModel?> GetModelAsync(string modelAlias, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelImplAsync(modelAlias, ct),
                                                     $"Error getting model with alias '{modelAlias}'.", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<IModel?> GetModelAsync(string modelAlias,
                                             string? preferCatalogRegistryName,
                                             CancellationToken? ct = null)
    {
        if (string.IsNullOrWhiteSpace(preferCatalogRegistryName))
        {
            return await GetModelAsync(modelAlias, ct).ConfigureAwait(false);
        }

        var model = await GetModelAsync(modelAlias, ct).ConfigureAwait(false);
        if (model == null || model.Info.IsFromCatalogRegistry(preferCatalogRegistryName))
        {
            return model;
        }

        // Prefer a variant from the named registry; pin it via GetModelVariantAsync
        // so callers get the single-variant IModel contract. Fall back to the
        // unfiltered model so the alias still resolves — preference is best-effort.
        var preferred = model.Variants.FirstOrDefault(v => v.Info.IsFromCatalogRegistry(preferCatalogRegistryName));
        return preferred != null
            ? await GetModelVariantAsync(preferred.Id, ct).ConfigureAwait(false) ?? model
            : model;
    }

    public async Task<IModel?> GetModelVariantAsync(string modelId, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(() => GetModelVariantImplAsync(modelId, ct),
                                                     $"Error getting model variant with ID '{modelId}'.", _logger)
                                                    .ConfigureAwait(false);
    }

    public async Task<IModel> GetLatestVersionAsync(IModel modelOrModelVariant, CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => GetLatestVersionImplAsync(modelOrModelVariant, ct),
            $"Error getting latest version for model with name '{modelOrModelVariant.Info.Name}'.",
            _logger).ConfigureAwait(false);
    }

    private async Task<List<IModel>> ListModelsImplAsync(CancellationToken? ct = null)
    {
        await UpdateModels(ct).ConfigureAwait(false);

        using var disposable = await _lock.LockAsync().ConfigureAwait(false);
        return _modelAliasToModel.Values.OrderBy(m => m.Alias).Cast<IModel>().ToList();
    }

    private async Task<List<IModel>> GetCachedModelsImplAsync(CancellationToken? ct = null)
    {
        var cachedModelIds = await Utils.GetCachedModelIdsAsync(_coreInterop, ct).ConfigureAwait(false);

        List<IModel> cachedModels = [];
        foreach (var modelId in cachedModelIds)
        {
            if (_modelIdToModelVariant.TryGetValue(modelId, out ModelVariant? modelVariant))
            {
                cachedModels.Add(modelVariant);
            }
        }

        return cachedModels;
    }

    private async Task<List<IModel>> GetLoadedModelsImplAsync(CancellationToken? ct = null)
    {
        var loadedModelIds = await _modelLoadManager.ListLoadedModelsAsync(ct).ConfigureAwait(false);
        List<IModel> loadedModels = [];

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

    private async Task<IModel> GetLatestVersionImplAsync(IModel modelOrModelVariant, CancellationToken? ct)
    {
        Model? model;

        if (modelOrModelVariant is ModelVariant)
        {
            // For ModelVariant, resolve the owning Model via alias.
            model = await GetModelImplAsync(modelOrModelVariant.Alias, ct);
        }
        else
        {
            // Try to use the concrete Model instance if this is our SDK type.
            model = modelOrModelVariant as Model;

            // If this is a different IModel implementation (e.g., a test stub),
            // fall back to resolving the Model via alias.
            if (model == null)
            {
                model = await GetModelImplAsync(modelOrModelVariant.Alias, ct);
            }
        }

        if (model == null)
        {
            throw new FoundryLocalException($"Model with alias '{modelOrModelVariant.Alias}' not found in catalog.",
                                            _logger);
        }

        // variants are sorted by version, so the first one matching the name is the latest version for that variant.
        var latest = model!.Variants.FirstOrDefault(v => v.Info.Name == modelOrModelVariant.Info.Name) ??
            // should not be possible given we internally manage all the state involved
            throw new FoundryLocalException($"Internal error. Mismatch between model (alias:{model.Alias}) and " +
                                            $"model variant (alias:{modelOrModelVariant.Alias}).", _logger);

        // if input was the latest return the input (could be model or model variant)
        // otherwise return the latest model variant
        return latest.Id == modelOrModelVariant.Id ? modelOrModelVariant : latest;
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

    internal void InvalidateCache()
    {
        _lastFetch = DateTime.MinValue;
    }

    // Year 3000 ≈ 32503680000 Unix seconds. A larger 'exp'/'iat' almost
    // certainly means the caller passed ToUnixTimeMilliseconds() by mistake;
    // MDS would later reject it with an opaque 401.
    private static void RejectMillisecondJwt(string bearer)
    {
        var t = bearer.Trim();
        if (t.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase)) { t = t.Substring(7).Trim(); }
        var parts = t.Split('.');
        if (parts.Length < 2) { return; }

        string b64 = parts[1].Replace('-', '+').Replace('_', '/');
        b64 += new string('=', (4 - b64.Length % 4) % 4);
        byte[] payload;
        try { payload = Convert.FromBase64String(b64); } catch (FormatException) { return; }

        try
        {
            using var doc = JsonDocument.Parse(payload);
            foreach (var claim in new[] { "exp", "iat" })
            {
                if (doc.RootElement.TryGetProperty(claim, out var v) &&
                    v.ValueKind == JsonValueKind.Number &&
                    v.TryGetInt64(out var n) && n > 32503680000L)
                {
                    throw new ArgumentException(
                        $"JWT '{claim}' claim ({n}) looks like milliseconds since epoch. " +
                        "Use DateTimeOffset.UtcNow.ToUnixTimeSeconds(), not ToUnixTimeMilliseconds().",
                        "options");
                }
            }
        }
        catch (JsonException) { }
    }

    private FoundryLocalException ClassifyCatalogError(string cmd, string err, CancellationToken? ct, string context)
    {
        var p = err.ToLowerInvariant();
        string? reason =
            p.Contains("expired") ? "ExpiredToken" :
            p.Contains("audience") || p.Contains("\"aud\"") ? "InvalidAudience" :
            p.Contains("registry_name") ? "MissingRegistryName" :
            p.Contains("signature") ? "InvalidSignature" :
            p.Contains("401") || p.Contains("unauthorized") ? "Unauthorized" :
            p.Contains("403") || p.Contains("forbidden") ? "Forbidden" :
            null;
        if (reason != null)
        {
            _logger.LogError("Catalog auth failure ({Reason}): {Err}", reason, err);
            return new CatalogAuthException($"{context}: {err} (reason: {reason})", reason);
        }
        return Utils.FromNativeError(cmd, err, ct, _logger, context: context);
    }

    public void Dispose()
    {
        _lock.Dispose();
    }

    public Task AddCatalogAsync(string name, Uri uri,
                                Dictionary<string, string>? options = null,
                                CancellationToken? ct = null)
        => AddOrUpdateCatalogAsync(name, uri, options, ct);

    public Task AddCatalogAsync(string name, Uri uri, PrivateCatalogOptions options,
                                CancellationToken? ct = null)
    {
        if (options is null) { throw new ArgumentNullException(nameof(options)); }
        var d = new Dictionary<string, string>();
        if (!string.IsNullOrEmpty(options.BearerToken)) { d["BearerToken"] = options.BearerToken!; }
        if (!string.IsNullOrEmpty(options.Audience)) { d["Audience"] = options.Audience!; }
        return AddOrUpdateCatalogAsync(name, uri, d, ct);
    }

    public async Task AddOrUpdateCatalogAsync(string name, Uri uri,
                                              Dictionary<string, string>? options = null,
                                              CancellationToken? ct = null)
    {
#if NET7_0_OR_GREATER
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentNullException.ThrowIfNull(uri);
#else
        if (string.IsNullOrWhiteSpace(name))
        {
            throw new ArgumentException("Catalog name must be a non-empty, non-whitespace string.", nameof(name));
        }
        if (uri is null)
        {
            throw new ArgumentNullException(nameof(uri));
        }
#endif

        if (uri.Scheme != "https" && uri.Scheme != "http")
        {
            throw new ArgumentException($"Catalog URI must use http or https scheme, got '{uri.Scheme}'.", nameof(uri));
        }

        if (options != null && options.TryGetValue("TokenEndpoint", out var tokenEndpoint) && tokenEndpoint != null)
        {
            if (!Uri.TryCreate(tokenEndpoint, UriKind.Absolute, out var parsedEndpoint))
            {
                throw new ArgumentException($"Token endpoint is not a valid URL: '{tokenEndpoint}'.");
            }
            if (parsedEndpoint.Scheme != "https" && parsedEndpoint.Scheme != "http")
            {
                throw new ArgumentException($"Token endpoint must use http or https scheme, got '{parsedEndpoint.Scheme}'.");
            }
        }

        // Fail fast on the common 'exp/iat in milliseconds' JWT mistake.
        if (options != null && options.TryGetValue("BearerToken", out var bearer) && !string.IsNullOrEmpty(bearer))
        {
            RejectMillisecondJwt(bearer!);
        }

        await Utils.CallWithExceptionHandling(async () =>
        {
            // Caller-supplied options first; Name/Uri/Type overlaid so they can't
            // be silently overridden. Default Type to AzurePrivate; honour an
            // explicit "Type" in options.
            var p = new Dictionary<string, string>(options ?? new Dictionary<string, string>())
            {
                ["Name"] = name,
                ["Uri"] = uri.ToString(),
            };
            if (!p.TryGetValue("Type", out var typeValue) || string.IsNullOrEmpty(typeValue))
            {
                p["Type"] = "AzurePrivate";
            }

            // Idempotent: if a catalog with this name already exists, remove it
            // first so re-registration (e.g. token refresh) is a no-op for the
            // happy path. Errors from remove are swallowed at debug \u2014 add_catalog
            // will surface the real problem.
            try
            {
                var rm = await _coreInterop.ExecuteCommandAsync(
                    "remove_catalog",
                    new CoreInteropRequest { Params = new Dictionary<string, string> { ["Name"] = name } },
                    ct).ConfigureAwait(false);
                if (rm.Error != null)
                {
                    _logger.LogDebug("remove_catalog('{Name}') before add returned: {Err}", name, rm.Error);
                }
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                _logger.LogDebug(ex, "remove_catalog('{Name}') before add threw (ignored).", name);
            }

            var add = await _coreInterop.ExecuteCommandAsync(
                "add_catalog", new CoreInteropRequest { Params = p }, ct).ConfigureAwait(false);
            if (add.Error != null)
            {
                throw ClassifyCatalogError("add_catalog", add.Error, ct, $"Error adding catalog '{name}'");
            }

            InvalidateCache();
            await UpdateModels(ct).ConfigureAwait(false);
        }, $"Error adding catalog '{name}'.", _logger).ConfigureAwait(false);
    }

    public async Task RemoveCatalogAsync(string name, CancellationToken? ct = null)
    {
        if (string.IsNullOrWhiteSpace(name))
        {
            throw new ArgumentException("Catalog name must be a non-empty, non-whitespace string.", nameof(name));
        }

        await Utils.CallWithExceptionHandling(async () =>
        {
            var request = new CoreInteropRequest { Params = new Dictionary<string, string> { ["Name"] = name } };
            var result = await _coreInterop.ExecuteCommandAsync("remove_catalog", request, ct).ConfigureAwait(false);
            if (result.Error != null)
            {
                throw Utils.FromNativeError("remove_catalog", result.Error, ct, _logger,
                                            context: $"Error removing catalog '{name}'");
            }
            InvalidateCache();
            await UpdateModels(ct).ConfigureAwait(false);
        }, $"Error removing catalog '{name}'.", _logger).ConfigureAwait(false);
    }

    public async Task<List<string>> GetCatalogNamesAsync(CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(async () =>
        {
            CoreInteropRequest? input = null;
            var result = await _coreInterop.ExecuteCommandAsync("get_catalog_names", input, ct)
                                           .ConfigureAwait(false);
            if (result.Error != null)
            {
                throw new FoundryLocalException($"Error getting catalog names: {result.Error}", _logger);
            }

            return JsonSerializer.Deserialize(result.Data ?? "[]", JsonSerializationContext.Default.ListString) ?? [];
        }, "Error getting catalog names.", _logger).ConfigureAwait(false);
    }
}
