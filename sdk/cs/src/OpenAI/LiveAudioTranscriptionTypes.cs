namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json;
using System.Text.Json.Serialization;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;
using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

/// <summary>
/// Transcription result for real-time audio streaming sessions.
/// Extends <see cref="AudioCreateTranscriptionResponse"/> to provide a consistent
/// output format with file-based transcription, while adding streaming-specific fields.
/// </summary>
public class LiveAudioTranscriptionResponse : AudioCreateTranscriptionResponse
{
    /// <summary>
    /// Whether this is a final or partial (interim) result.
    /// - Nemotron models always return <c>true</c> (every result is final).
    /// - Other models (e.g., Azure Embedded) may return <c>false</c> for interim
    ///   hypotheses that will be replaced by a subsequent final result.
    /// </summary>
    [JsonPropertyName("is_final")]
    public bool IsFinal { get; init; }

    internal static LiveAudioTranscriptionResponse FromJson(string json)
    {
        // Deserialize the core's JSON (which has is_final, text, start_time, end_time)
        // into an intermediate record, then map to the response type.
        var raw = JsonSerializer.Deserialize(json,
            JsonSerializationContext.Default.LiveAudioTranscriptionRaw)
            ?? throw new FoundryLocalException("Failed to deserialize live audio transcription result");

        var response = new LiveAudioTranscriptionResponse
        {
            Text = raw.Text,
            IsFinal = raw.IsFinal,
        };

        // Map start_time/end_time into a Segment for OpenAI-compatible output
        if (raw.StartTime.HasValue || raw.EndTime.HasValue)
        {
            response.Segments =
            [
                new Segment
                {
                    Start = (float)(raw.StartTime ?? 0),
                    End = (float)(raw.EndTime ?? 0),
                    Text = raw.Text
                }
            ];
        }

        return response;
    }
}

/// <summary>
/// Internal raw deserialization target matching the Core's JSON format.
/// Mapped to <see cref="LiveAudioTranscriptionResponse"/> in FromJson.
/// </summary>
internal record LiveAudioTranscriptionRaw
{
    [JsonPropertyName("is_final")]
    public bool IsFinal { get; init; }

    [JsonPropertyName("text")]
    public string Text { get; init; } = string.Empty;

    [JsonPropertyName("start_time")]
    public double? StartTime { get; init; }

    [JsonPropertyName("end_time")]
    public double? EndTime { get; init; }
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