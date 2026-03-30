// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Text.Json.Serialization;

/// <summary>
/// Information about a discoverable execution provider.
/// </summary>
public class EpInfo
{
    /// <summary>
    /// The name of the execution provider (e.g. "CUDAExecutionProvider").
    /// </summary>
    [JsonPropertyName("Name")]
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// Whether the execution provider is currently registered.
    /// </summary>
    [JsonPropertyName("IsRegistered")]
    public bool IsRegistered { get; set; }
}
