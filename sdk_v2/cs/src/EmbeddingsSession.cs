// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// An embeddings session for text-embedding models.
/// Validates the model task at construction time.
/// Stateless and concurrent — multiple requests can be processed in parallel against
/// the same loaded model.
/// </summary>
public sealed class EmbeddingsSession : Session
{
    /// <summary>
    /// Create an embeddings session from a loaded embeddings model.
    /// An embeddings session accepts <see cref="TextItem"/> inputs and produces
    /// one <see cref="TensorItem"/> per input containing the embedding vector.
    /// </summary>
    /// <param name="model">A loaded model whose task is "embeddings".</param>
    /// <exception cref="ArgumentException">If the model's task is not embeddings.</exception>
    public EmbeddingsSession(IModel model) : base(model)
    {
        if (model.Info.Task != "embeddings")
        {
            throw new ArgumentException(
                $"EmbeddingsSession requires a model with task 'embeddings', but got '{model.Info.Task}'.",
                nameof(model));
        }
    }
}
