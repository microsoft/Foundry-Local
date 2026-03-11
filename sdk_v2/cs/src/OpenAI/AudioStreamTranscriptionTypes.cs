namespace Microsoft.AI.Foundry.Local;

using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.AI.Foundry.Local.Detail;

public record AudioStreamTranscriptionResult
{
    /// <summary>
    /// Whether this is a final or partial (interim) result.
    /// - Nemotron models always return <c>true</c> (every result is final).
    /// - Other models (e.g., Azure Embedded) may return <c>false</c> for interim
    ///   hypotheses that will be replaced by a subsequent final result.
    /// </summary>
    [JsonPropertyName("is_final")]
    public bool IsFinal { get; init; }

    /// <summary>
    /// Newly transcribed text from this audio chunk only (incremental hypothesis).
    /// This is NOT the full accumulated transcript — each result contains only
    /// the text decoded from the most recent audio chunk.
    /// </summary>
    [JsonPropertyName("text")]
    public string Text { get; init; } = string.Empty;

    /// <summary>Start time offset of this segment in the audio stream (seconds).</summary>
    [JsonPropertyName("start_time")]
    public double? StartTime { get; init; }

    /// <summary>End time offset of this segment in the audio stream (seconds).</summary>
    [JsonPropertyName("end_time")]
    public double? EndTime { get; init; }

    /// <summary>Confidence score (0.0 - 1.0) if available.</summary>
    [JsonPropertyName("confidence")]
    public float? Confidence { get; init; }

    internal static AudioStreamTranscriptionResult FromJson(string json)
    {
        return JsonSerializer.Deserialize(json,
            JsonSerializationContext.Default.AudioStreamTranscriptionResult)
            ?? throw new FoundryLocalException("Failed to deserialize AudioStreamTranscriptionResult");
    }
}

internal record CoreErrorResponse
{
    [JsonPropertyName("code")]
    public string Code { get; init; } = "";

    [JsonPropertyName("message")]
    public string Message { get; init; } = "";

    [JsonPropertyName("isTransient")]
    public bool IsTransient { get; init; }

    /// <summary>
    /// Attempt to parse a native error string as structured JSON.
    /// Returns null if the error is not valid JSON or doesn't match the schema,
    /// which should be treated as a permanent/unknown error.
    /// </summary>
    internal static CoreErrorResponse? TryParse(string errorString)
    {
        try
        {
            return JsonSerializer.Deserialize(errorString,
                JsonSerializationContext.Default.CoreErrorResponse);
        }
        catch
        {
            return null; // unstructured error — treat as permanent
        }
    }
}