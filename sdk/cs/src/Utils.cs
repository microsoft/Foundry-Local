// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;
using System.Text.Json;
using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging;

internal class Utils
{
    internal static async Task<string[]> GetCachedModelIdsAsync(ICoreInterop coreInterop, CancellationToken? ct = null)
    {
        CoreInteropRequest? input = null;
        var result = await coreInterop.ExecuteCommandAsync("get_cached_models", input, ct).ConfigureAwait(false);
        if (result.Error != null)
        {
            throw new FoundryLocalException($"Error getting cached model ids: {result.Error}");
        }

        var typeInfo = JsonSerializationContext.Default.StringArray;
        var cachedModelIds = JsonSerializer.Deserialize(result.Data!, typeInfo);
        if (cachedModelIds == null)
        {
            throw new FoundryLocalException($"Failed to deserialized cached model names. Json:'{result.Data!}'");
        }

        return cachedModelIds;
    }

    // Helper to wrap function calls with consistent exception handling
    internal static T CallWithExceptionHandling<T>(Func<T> func, string errorMsg, ILogger logger)
    {
        try
        {
            return func();
        }
        // we ignore OperationCanceledException to allow proper cancellation propagation
        // this also covers TaskCanceledException since it derives from OperationCanceledException
        catch (Exception ex)
            when (ex is not OperationCanceledException and not FoundryLocalException)
        {
            throw new FoundryLocalException(errorMsg, ex, logger);
        }
    }

    /// <summary>
    /// Build a <see cref="FoundryLocalException"/> from a native broker error string.
    /// Adds a hint when the native layer reports "cancelled" but the caller did not
    /// actually cancel — almost always a server/auth/network failure mis-reported as
    /// cancellation. Stashes raw error + command on <see cref="Exception.Data"/>.
    /// </summary>
    internal static FoundryLocalException FromNativeError(
        string commandName, string nativeError, CancellationToken? ct, ILogger logger, string? context = null)
    {
        var prefix = context ?? $"Error executing '{commandName}'";
        var userCancelled = ct.HasValue && ct.Value.IsCancellationRequested;
#if NETSTANDARD2_0
        var looksCancel = nativeError.IndexOf("cancel", System.StringComparison.OrdinalIgnoreCase) >= 0;
#else
        var looksCancel = nativeError.Contains("cancel", System.StringComparison.OrdinalIgnoreCase);
#endif
        var message = (looksCancel && !userCancelled)
            ? $"{prefix}: {nativeError}. Caller did not cancel — likely a server-side failure " +
              "(authentication, expired credentials, or network error)."
            : $"{prefix}: {nativeError}";

        logger.LogError("Native command '{Command}' failed: {NativeError} (caller cancelled: {UserCancelled})",
                        commandName, nativeError, userCancelled);

        var ex = new FoundryLocalException(message);
        ex.Data["Command"] = commandName;
        ex.Data["NativeError"] = nativeError;
        ex.Data["UserCancelled"] = userCancelled;
        return ex;
    }
}

