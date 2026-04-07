// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Text.Json.Serialization;

/// <summary>
/// Describes a discoverable execution provider bootstrapper.
/// </summary>
public record EpInfo
{
    /// <summary>The identifier of the bootstrapper/execution provider (e.g. "CUDAExecutionProvider").</summary>
    [JsonPropertyName("Name")]
    public required string Name { get; init; }

    /// <summary>True if this EP has already been successfully downloaded and registered.</summary>
    [JsonPropertyName("IsRegistered")]
    public required bool IsRegistered { get; init; }
}

/// <summary>
/// Result of an explicit EP download and registration operation.
/// </summary>
public record EpDownloadResult
{
    /// <summary>True if all requested EPs were successfully downloaded and registered.</summary>
    [JsonPropertyName("Success")]
    public required bool Success { get; init; }

    /// <summary>Human-readable status message.</summary>
    [JsonPropertyName("Status")]
    public required string Status { get; init; }

    /// <summary>Names of EPs that were successfully registered.</summary>
    [JsonPropertyName("RegisteredEps")]
    public required string[] RegisteredEps { get; init; }

    /// <summary>Names of EPs that failed to register.</summary>
    [JsonPropertyName("FailedEps")]
    public required string[] FailedEps { get; init; }
}
