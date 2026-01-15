// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Text.Json.Serialization;

[JsonConverter(typeof(JsonStringEnumConverter<DeviceType>))]
public enum DeviceType
{
    Invalid,
    CPU,
    GPU,
    NPU
}

public record PromptTemplate
{
    [JsonPropertyName("system")]
    public string? System { get; init; }

    [JsonPropertyName("user")]
    public string? User { get; init; }

    [JsonPropertyName("assistant")]
    public string Assistant { get; init; } = default!;

    [JsonPropertyName("prompt")]
    public string Prompt { get; init; } = default!;
}

public record Runtime
{
    [JsonPropertyName("deviceType")]
    public DeviceType DeviceType { get; init; } = default!;

    // there are many different possible values; keep it open‑ended
    [JsonPropertyName("executionProvider")]
    public string ExecutionProvider { get; init; } = default!;
}

public record Parameter
{
    public required string Name { get; set; }
    public string? Value { get; set; }
}

public record ModelSettings
{
    [JsonPropertyName("parameters")]
    public Parameter[]? Parameters { get; set; }
}

public record ModelInfo
{
    [JsonPropertyName("id")]
    public required string Id { get; init; }

    [JsonPropertyName("name")]
    public required string Name { get; init; }

    [JsonPropertyName("version")]
    public int Version { get; init; }

    [JsonPropertyName("alias")]
    public required string Alias { get; init; }

    [JsonPropertyName("displayName")]
    public string? DisplayName { get; init; }

    [JsonPropertyName("providerType")]
    public required string ProviderType { get; init; }

    [JsonPropertyName("uri")]
    public required string Uri { get; init; }

    [JsonPropertyName("modelType")]
    public required string ModelType { get; init; }

    [JsonPropertyName("promptTemplate")]
    public PromptTemplate? PromptTemplate { get; init; }

    [JsonPropertyName("publisher")]
    public string? Publisher { get; init; }

    [JsonPropertyName("modelSettings")]
    public ModelSettings? ModelSettings { get; init; }

    [JsonPropertyName("license")]
    public string? License { get; init; }

    [JsonPropertyName("licenseDescription")]
    public string? LicenseDescription { get; init; }

    [JsonPropertyName("cached")]
    public bool Cached { get; init; }


    [JsonPropertyName("task")]
    public string? Task { get; init; }

    [JsonPropertyName("runtime")]
    public Runtime? Runtime { get; init; }

    [JsonPropertyName("fileSizeMb")]
    public int? FileSizeMb { get; init; }

    [JsonPropertyName("supportsToolCalling")]
    public bool? SupportsToolCalling { get; init; }

    [JsonPropertyName("maxOutputTokens")]
    public long? MaxOutputTokens { get; init; }

    [JsonPropertyName("minFLVersion")]
    public string? MinFLVersion { get; init; }

    [JsonPropertyName("createdAt")]
    public long CreatedAtUnix { get; init; }
}
