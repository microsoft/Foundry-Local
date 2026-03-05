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
    /// <returns>The matching Model, or null if no model with the given alias exists.</returns>
    Task<Model?> GetModelAsync(string modelAlias, CancellationToken? ct = null);

    /// <summary>
    /// Lookup a model variant by its unique model id.
    /// </summary>
    /// <param name="modelId">Model id.</param>
    /// <param name="ct">Optional CancellationToken.</param>
    /// <returns>The matching ModelVariant, or null if no variant with the given id exists.</returns>
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
}
