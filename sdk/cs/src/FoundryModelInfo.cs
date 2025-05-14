using System.Text.Json.Serialization;

namespace FoundryLocal;

public enum DeviceType
{
    CPU,
    GPU,
    NPU,
}

public enum ExecutionProvider
{
    CPU,
    WEBGPU,
    CUDA,
    QNN
}

public interface IModelRuntime
{
    public DeviceType DeviceType { get; }
    public ExecutionProvider ExecutionProvider { get; }
}

public class ModelInfo
{
    [JsonPropertyName("shortName")]
    public required string Alias { get; init; }

    public required string Id { get; init; }

    public required string Version { get; init; }

    [JsonPropertyName("name")]
    public required ExecutionProvider Runtime { get; init; }

    public required string Uri { get; init; }

    public required int FileSizeMb { get; init; }
    public required string? PromptTemplate { get; init; }

    [JsonPropertyName("providerType")]
    public required string Provider { get; init; }
}

internal class DownloadRequest
{
    internal class ModelInfo
    {
        public required string Name { get; set; }
        public required string Uri { get; set; }
        public required string Path { get; set; }
        public required string ProviderType { get; set; }
        public required string PromptTemplate { get; set; }
    }

    [JsonPropertyName("model")]
    public required ModelInfo Model { get; set; }

    [JsonPropertyName("token")]
    public required string Token { get; set; }

    [JsonPropertyName("IgnorePipeReport")]
    public required bool IgnorePipeReport { get; set; }

}

[JsonSerializable(typeof(ModelInfo))]
[JsonSerializable(typeof(List<ModelInfo>))]
[JsonSerializable(typeof(int))]
public partial class ModelGenerationContext : JsonSerializerContext
{
}
