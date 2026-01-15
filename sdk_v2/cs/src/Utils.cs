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
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            if (ex is FoundryLocalException)
            {
                throw;
            }
            throw new FoundryLocalException(errorMsg, ex, logger);
        }
    }
}
