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
    /// <returns>List of IModel instances.</returns>
    Task<List<IModel>> ListModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model by its alias.
    /// </summary>
    /// <param name="modelAlias">Model alias.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>The matching IModel, or null if no model with the given alias exists.</returns>
    Task<IModel?> GetModelAsync(string modelAlias, CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model variant by its unique model id.
    /// NOTE: This will return an IModel with a single variant. Use GetModelAsync to get an IModel with all available
    ///       variants.
    /// </summary>
    /// <param name="modelId">Model id.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>The matching IModel, or null if no variant with the given id exists.</returns>
    Task<IModel?> GetModelVariantAsync(string modelId, CancellationToken? ct = null);

    /// <summary>
    /// Get a list of currently downloaded models from the model cache.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of IModel instances.</returns>
    Task<List<IModel>> GetCachedModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Get a list of the currently loaded models.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of IModel instances.</returns>
    Task<List<IModel>> GetLoadedModelsAsync(CancellationToken? ct = null);

    /// <summary>
    /// Get the latest version of a model.
    /// This is used to check if a newer version of a model is available in the catalog for download.
    /// </summary>
    /// <param name="model">The model to check for the latest version.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>The latest version of the model. Will match the input if it is the latest version.</returns>
    Task<IModel> GetLatestVersionAsync(IModel model, CancellationToken? ct = null);

    /// <summary>
    /// Add a private model catalog. The model list is refreshed automatically,
    /// so models from the new catalog are available as soon as this call returns.
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
