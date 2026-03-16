// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System.Collections.Generic;

public interface ICatalog
{
    /// <summary>
    /// The catalog name.
    /// </summary>
    string Name { get; }

    /// <summary>
    /// List the available models in the catalog.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of Model instances.</returns>
    Task<List<Model>> ListModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model by its alias.
    /// </summary>
    /// <param name="modelAlias">Model alias.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>Model if found.</returns>
    Task<Model?> GetModelAsync(string modelAlias, CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model variant by its unique model id.
    /// </summary>
    /// <param name="modelId">Model id.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>Model variant if found.</returns>
    Task<ModelVariant?> GetModelVariantAsync(string modelId, CancellationToken? ct = null);

    /// <summary>
    /// Get a list of currently downloaded models from the model cache.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of ModelVariant instances.</returns>
    Task<List<ModelVariant>> GetCachedModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Get a list of the currently loaded models.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of ModelVariant instances.</returns>
    Task<List<ModelVariant>> GetLoadedModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Add a private model catalog. Models from the new catalog become available
    /// on the next ListModelsAsync or GetModelAsync call.
    /// </summary>
    /// <param name="name">Display name for the catalog (e.g. "my-private-catalog").</param>
    /// <param name="uri">Base URL of the private catalog service.</param>
    /// <param name="clientId">Optional OAuth2 client credentials ID.</param>
    /// <param name="clientSecret">Optional OAuth2 client credentials secret, or API key for legacy auth.</param>
    /// <param name="bearerToken">Optional pre-obtained bearer token (for testing/self-service auth).</param>
    /// <param name="tokenEndpoint">Optional OAuth2 token endpoint URL (e.g. "https://idp.example.com/oauth/token").</param>
    /// <param name="audience">Optional OAuth2 audience parameter (e.g. "model-distribution-service").</param>
    /// <param name="ct">Optional CancellationToken.</param>
    Task AddCatalogAsync(string name, Uri uri, string? clientId = null, string? clientSecret = null,
                         string? bearerToken = null, string? tokenEndpoint = null, string? audience = null,
                         CancellationToken? ct = null);

    /// <summary>
    /// Filter the catalog to only return models from the named catalog.
    /// Pass null to reset and show models from all catalogs.
    /// </summary>
    /// <param name="catalogName">Catalog name to filter to, or null to show all.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    Task SelectCatalogAsync(string? catalogName, CancellationToken? ct = null);

    /// <summary>
    /// Get the names of all registered catalogs.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of catalog name strings.</returns>
    Task<List<string>> GetCatalogNamesAsync(CancellationToken? ct = null);
}
