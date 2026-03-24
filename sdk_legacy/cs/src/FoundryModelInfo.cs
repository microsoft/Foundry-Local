// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Text.Json;
using System.Text.Json.Serialization;

public record PromptTemplate
{
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

public record ModelSettings
{
    // The sample shows an empty array; keep it open‑ended.
    [JsonPropertyName("parameters")]
    public List<JsonElement> Parameters { get; init; } = [];
}

public record FoundryCachedModel(string Name, string? Id);

public record FoundryDownloadResult(bool Success, string? ErrorMessage);

public record ModelInfo
{
    [JsonPropertyName("name")]
    public string ModelId { get; init; } = default!;

    [JsonPropertyName("displayName")]
    public string DisplayName { get; init; } = default!;

    [JsonPropertyName("providerType")]
    public string ProviderType { get; init; } = default!;

    [JsonPropertyName("uri")]
    public string Uri { get; init; } = default!;

    [JsonPropertyName("version")]
    public string Version { get; init; } = default!;

    [JsonPropertyName("modelType")]
    public string ModelType { get; init; } = default!;

    [JsonPropertyName("promptTemplate")]
    public PromptTemplate? PromptTemplate { get; init; }

    [JsonPropertyName("publisher")]
    public string Publisher { get; init; } = default!;

    [JsonPropertyName("task")]
    public string Task { get; init; } = default!;

    [JsonPropertyName("runtime")]
    public Runtime Runtime { get; init; } = default!;

    [JsonPropertyName("fileSizeMb")]
    public long FileSizeMb { get; init; }

    [JsonPropertyName("modelSettings")]
    public ModelSettings ModelSettings { get; init; } = default!;

    [JsonPropertyName("alias")]
    public string Alias { get; init; } = default!;

    [JsonPropertyName("supportsToolCalling")]
    public bool SupportsToolCalling { get; init; }

    [JsonPropertyName("license")]
    public string License { get; init; } = default!;

    [JsonPropertyName("licenseDescription")]
    public string LicenseDescription { get; init; } = default!;

    [JsonPropertyName("parentModelUri")]
    public string ParentModelUri { get; init; } = default!;

    [JsonPropertyName("maxOutputTokens")]
    public long MaxOutputTokens { get; init; }

    [JsonPropertyName("minFLVersion")]
    public string MinFLVersion { get; init; } = default!;

    [JsonPropertyName("epOverride")]
    public string? EpOverride { get; set; }
}

internal sealed class DownloadRequest
{
    internal sealed class ModelInfo
    {
        [JsonPropertyName("Name")]
        public required string Name { get; set; }
        [JsonPropertyName("Uri")]
        public required string Uri { get; set; }
        [JsonPropertyName("Publisher")]
        public required string Publisher { get; set; }
        [JsonPropertyName("ProviderType")]
        public required string ProviderType { get; set; }
        [JsonPropertyName("PromptTemplate")]
        public required PromptTemplate? PromptTemplate { get; set; }
    }

    [JsonPropertyName("Model")]
    public required ModelInfo Model { get; set; }

    [JsonPropertyName("token")]
    public required string Token { get; set; }

    [JsonPropertyName("IgnorePipeReport")]
    public required bool IgnorePipeReport { get; set; }
}

public record ModelDownloadProgress
{
    public double Percentage { get; init; }
    public bool IsCompleted { get; init; }
    public ModelInfo? ModelInfo { get; init; }
    public string? ErrorMessage { get; init; }

    public static ModelDownloadProgress Progress(double percentage) =>
        new()
        { Percentage = percentage, IsCompleted = false };

    public static ModelDownloadProgress Completed(ModelInfo modelInfo) =>
        new()
        { Percentage = 100, IsCompleted = true, ModelInfo = modelInfo };

    public static ModelDownloadProgress Error(string errorMessage) =>
        new()
        { IsCompleted = true, ErrorMessage = errorMessage };
}

[JsonSerializable(typeof(ModelInfo))]
[JsonSerializable(typeof(List<ModelInfo>))]
[JsonSerializable(typeof(int))]
[JsonSerializable(typeof(ModelDownloadProgress))]
public partial class ModelGenerationContext : JsonSerializerContext
{
}
