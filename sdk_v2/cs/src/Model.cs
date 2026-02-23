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

    public List<ModelVariant> Variants { get; internal set; }
    public ModelVariant SelectedVariant { get; internal set; } = default!;

    public string Alias { get; init; }
    public string Id => SelectedVariant.Id;

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
        Variants = new() { modelVariant };

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

        Variants.Add(variant);

        // prefer the highest priority locally cached variant
        if (variant.Info.Cached && !SelectedVariant.Info.Cached)
        {
            SelectedVariant = variant;
        }
    }

    /// <summary>
    /// Select a specific model variant by its unique model ID.
    /// The selected variant will be used for <see cref="IModel"/> operations.
    /// </summary>
    /// <param name="variant">Model variant to select.</param>
    /// <exception cref="FoundryLocalException">If variant is not valid for this model.</exception>
    public void SelectVariant(ModelVariant variant)
    {
        _ = Variants.FirstOrDefault(v => v == variant) ??
            // user error so don't log
            throw new FoundryLocalException($"Model {Alias} does not have a {variant.Id} variant.");

        SelectedVariant = variant;
    }

    /// <summary>
    /// Get the latest version of the specified model variant.
    /// </summary>
    /// <param name="variant">Model variant.</param>
    /// <returns>ModelVariant for latest version. Same as `variant` if that is the latest version.</returns>
    /// <exception cref="FoundryLocalException">If variant is not valid for this model.</exception>
    public ModelVariant GetLatestVersion(ModelVariant variant)
    {
        // variants are sorted by version, so the first one matching the name is the latest version for that variant.
        var latest = Variants.FirstOrDefault(v => v.Info.Name == variant.Info.Name) ??
            // user error so don't log
            throw new FoundryLocalException($"Model {Alias} does not have a {variant.Id} variant.");

        return latest;
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

    public async Task UnloadAsync(CancellationToken? ct = null)
    {
        await SelectedVariant.UnloadAsync(ct).ConfigureAwait(false);
    }

    public async Task RemoveFromCacheAsync(CancellationToken? ct = null)
    {
        await SelectedVariant.RemoveFromCacheAsync(ct).ConfigureAwait(false);
    }
}
