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
    /// List the available models filtered to those whose <see cref="ModelInfo.RegistryName"/>
    /// matches <paramref name="catalogRegistryName"/> (case-insensitive).
    /// </summary>
    /// <param name="catalogRegistryName">
    /// Registry name to filter by (e.g. <c>mds-acme-registry</c> for an MDS-hosted
    /// private catalog, or the name of the Azure ML public registry). Required.
    /// </param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of IModel instances whose URI is rooted at the given registry.</returns>
    /// <remarks>
    /// This is a client-side filter on the result of <see cref="ListModelsAsync(CancellationToken?)"/>;
    /// no extra round-trips to the broker. Useful for samples / UIs that want to
    /// group "public catalog" vs "private catalog" models.
    /// </remarks>
    Task<List<IModel>> ListModelsAsync(string catalogRegistryName, CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model by its alias.
    /// </summary>
    /// <param name="modelAlias">Model alias.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>The matching IModel, or null if no model with the given alias exists.</returns>
    Task<IModel?> GetModelAsync(string modelAlias, CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model by its alias, preferring variants that originate from
    /// the named catalog registry. If no variants match the preferred registry,
    /// falls back to the unfiltered result (same as <see cref="GetModelAsync(string, CancellationToken?)"/>).
    /// </summary>
    /// <param name="modelAlias">Model alias.</param>
    /// <param name="preferCatalogRegistryName">
    /// Catalog registry name to prefer (e.g. <c>mds-acme-registry</c>). When the
    /// same alias is published by both the public and a private catalog this
    /// disambiguates which one the caller wants. Null or empty disables the
    /// preference and behaves like the single-argument overload.
    /// </param>
    /// <param name="ct">Optional CancellationToken.</param>
    Task<IModel?> GetModelAsync(string modelAlias,
                                string? preferCatalogRegistryName,
                                CancellationToken? ct = null);

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
    /// <param name="options">Optional authentication and configuration parameters (e.g. ClientId, ClientSecret, BearerToken, TokenEndpoint, Audience). Pass "Type" to override the default catalog type ("AzurePrivate").</param>
    /// <param name="ct">Optional CancellationToken.</param>
    Task AddCatalogAsync(string name, Uri uri, Dictionary<string, string>? options = null,
                         CancellationToken? ct = null);

    /// <summary>
    /// Idempotent variant of <see cref="AddCatalogAsync"/>: if a catalog with the
    /// same name already exists it is removed and re-added with the supplied
    /// options. Use this to refresh credentials (e.g. rotate an expired
    /// <c>BearerToken</c>) without restarting the SDK.
    /// </summary>
    Task AddOrUpdateCatalogAsync(string name, Uri uri, Dictionary<string, string>? options = null,
                                 CancellationToken? ct = null);

    /// <summary>
    /// Remove a previously-added private catalog by name. No-op for the built-in
    /// public catalog. After removal the model list is refreshed so cached
    /// models from the removed catalog no longer appear in <see cref="ListModelsAsync(CancellationToken?)"/>.
    /// </summary>
    Task RemoveCatalogAsync(string name, CancellationToken? ct = null);

    /// <summary>
    /// Get the names of all registered catalogs.
    /// </summary>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>List of catalog name strings.</returns>
    Task<List<string>> GetCatalogNamesAsync(CancellationToken? ct = null);
}
