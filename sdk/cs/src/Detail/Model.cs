// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using Microsoft.Extensions.Logging;

public class Model : IModel
{
    private readonly ILogger _logger;

    private readonly List<IModel> _variants;
    public IReadOnlyList<IModel> Variants => _variants;
    internal IModel SelectedVariant { get; set; } = default!;

    public string Alias { get; init; }
    public string Id => SelectedVariant.Id;
    public ModelInfo Info => SelectedVariant.Info;

    /// <summary>
    /// Is the currently selected variant cached locally?
    /// </summary>
    public Task<bool> IsCachedAsync(CancellationToken? ct = null) => SelectedVariant.IsCachedAsync(ct);

    /// <summary>
    /// Is the currently selected variant loaded in memory?
    /// </summary>
    public Task<bool> IsLoadedAsync(CancellationToken? ct = null) => SelectedVariant.IsLoadedAsync(ct);

    internal Model(ModelVariant modelVariant, ILogger logger)
    {
        _logger = logger;

        Alias = modelVariant.Alias;
        _variants = [modelVariant];

        // variants are sorted by Core, so the first one added is the default
        SelectedVariant = modelVariant;
    }

    internal void AddVariant(ModelVariant variant)
    {
        if (Alias != variant.Alias)
        {
            // internal error so log
            throw new FoundryLocalException($"Variant alias {variant.Alias} does not match model alias {Alias}",
                                            _logger);
        }

        _variants.Add(variant);

        // prefer the highest priority locally cached variant
        if (variant.Info.Cached && !SelectedVariant.Info.Cached)
        {
            SelectedVariant = variant;
        }
    }

    /// <summary>
    /// Select a specific model variant from <see cref="Variants"/> to use for <see cref="IModel"/> operations.
    /// </summary>
    /// <param name="variant">Model variant to select. Must be one of the variants in <see cref="Variants"/>.</param>
    /// <exception cref="FoundryLocalException">If variant is not valid for this model.</exception>
    public void SelectVariant(IModel variant)
    {
        _ = Variants.FirstOrDefault(v => v == variant) ??
            // user error so don't log. 
            throw new FoundryLocalException($"Input variant was not found in Variants.");

        SelectedVariant = variant;
    }

    public async Task<string> GetPathAsync(CancellationToken? ct = null)
    {
        return await SelectedVariant.GetPathAsync(ct).ConfigureAwait(false);
    }

    public async Task DownloadAsync(Action<float>? downloadProgress = null,
                                    CancellationToken? ct = null)
    {
        await SelectedVariant.DownloadAsync(downloadProgress, ct).ConfigureAwait(false);
    }

    public async Task LoadAsync(CancellationToken? ct = null)
    {
        await SelectedVariant.LoadAsync(ct).ConfigureAwait(false);
    }

    public async Task<OpenAIChatClient> GetChatClientAsync(CancellationToken? ct = null)
    {
        return await SelectedVariant.GetChatClientAsync(ct).ConfigureAwait(false);
    }

    public async Task<OpenAIAudioClient> GetAudioClientAsync(CancellationToken? ct = null)
    {
        return await SelectedVariant.GetAudioClientAsync(ct).ConfigureAwait(false);
    }

    public async Task<OpenAIEmbeddingClient> GetEmbeddingClientAsync(CancellationToken? ct = null)
    {
        return await SelectedVariant.GetEmbeddingClientAsync(ct).ConfigureAwait(false);
    }

    public async Task UnloadAsync(CancellationToken? ct = null)
    {
        await SelectedVariant.UnloadAsync(ct).ConfigureAwait(false);
    }

    public async Task RemoveFromCacheAsync(CancellationToken? ct = null)
    {
        await SelectedVariant.RemoveFromCacheAsync(ct).ConfigureAwait(false);
    }
}
