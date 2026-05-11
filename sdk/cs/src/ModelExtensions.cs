// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

public static class ModelExtensions
{
    /// <summary>
    /// Download the model to local cache if not already present.
    /// </summary>
    /// <param name="model">Model to download.</param>
    /// <param name="ct">Cancellation token.</param>
    public static Task DownloadAsync(this IModel model, CancellationToken ct)
    {
        return model.DownloadAsync(null, ct);
    }

    /// <summary>
    /// Download the model to local cache if not already present.
    /// </summary>
    /// <param name="model">Model to download.</param>
    /// <param name="downloadProgress">
    /// Optional progress callback for download progress.
    /// Percentage download (0 - 100.0) is reported.</param>
    /// <param name="ct">Cancellation token.</param>
    public static Task DownloadAsync(this IModel model, Action<float>? downloadProgress, CancellationToken ct)
    {
#if NET8_0_OR_GREATER
        ArgumentNullException.ThrowIfNull(model);
#else
        if (model == null)
        {
            throw new ArgumentNullException(nameof(model));
        }
#endif

        return model switch
        {
            Model sdkModel => sdkModel.DownloadAsync(downloadProgress, ct),
            ModelVariant sdkVariant => sdkVariant.DownloadAsync(downloadProgress, ct),
            _ => DownloadFallbackAsync(model, downloadProgress, ct),
        };
    }

    private static Task DownloadFallbackAsync(IModel model, Action<float>? downloadProgress, CancellationToken ct)
    {
        ct.ThrowIfCancellationRequested();
        return model.DownloadAsync(downloadProgress);
    }
}
