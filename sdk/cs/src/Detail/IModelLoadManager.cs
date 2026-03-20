// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;
using System.Threading.Tasks;

/// <summary>
/// Interface for model load management.
/// These operations can be done directly or via the optional web service.
/// </summary>
internal interface IModelLoadManager
{
    internal abstract Task LoadAsync(string modelName, CancellationToken? ct = null);
    internal abstract Task UnloadAsync(string modelName, CancellationToken? ct = null);
    internal abstract Task<string[]> ListLoadedModelsAsync(CancellationToken? ct = null);
}
