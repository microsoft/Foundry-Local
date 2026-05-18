// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System.Collections.Generic;

/// <summary>
/// Strongly-typed options for <see cref="ICatalog.AddCatalogAsync(string, PrivateCatalogOptions, System.Threading.CancellationToken?)"/>.
/// </summary>
public sealed class PrivateCatalogOptions
{
    /// <summary>JWT bearer token. <c>exp</c>/<c>iat</c> MUST be Unix seconds, not milliseconds.</summary>
    public string? BearerToken { get; set; }
    /// <summary>JWT <c>aud</c> claim MDS expects (e.g. "model-distribution-service").</summary>
    public string? Audience { get; set; }
}

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
    /// Add a private model catalog. The endpoint is fixed (the SDK targets the
    /// Model Distribution Service); only <paramref name="name"/> and credentials
    /// are caller-provided. Idempotent: calling with the same
    /// <paramref name="name"/> replaces the existing registration (use this to
    /// rotate an expired <c>BearerToken</c>). The model list is refreshed
    /// before returning.
    /// </summary>
    /// <param name="options">
    /// Recognised keys: <c>BearerToken</c>, <c>Audience</c>, <c>TokenEndpoint</c>,
    /// <c>ClientId</c>, <c>ClientSecret</c>, <c>Type</c> (default
    /// <c>"AzurePrivate"</c>). If <c>BearerToken</c> is a JWT, its <c>exp</c>/<c>iat</c>
    /// MUST be Unix seconds (not milliseconds); the SDK rejects ms-shaped values.
    /// Prefer the <see cref="PrivateCatalogOptions"/> overload for IntelliSense.
    /// </param>
    /// <exception cref="ArgumentException">Bad name, or JWT exp/iat in milliseconds.</exception>
    /// <exception cref="CatalogAuthException">MDS rejected the bearer token.</exception>
    Task AddCatalogAsync(string name, Dictionary<string, string>? options = null,
                         CancellationToken? ct = null);

    /// <summary>Strongly-typed overload of <see cref="AddCatalogAsync(string, Dictionary{string, string}?, CancellationToken?)"/>.</summary>
    Task AddCatalogAsync(string name, PrivateCatalogOptions options,
                         CancellationToken? ct = null);

    /// <summary>Alias for <see cref="AddCatalogAsync"/>; same idempotent behavior.</summary>
    Task AddOrUpdateCatalogAsync(string name, Dictionary<string, string>? options = null,
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
