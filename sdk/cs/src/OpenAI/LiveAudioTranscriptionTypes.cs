// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;

/// <summary>
/// A content part within a transcription result, following the OpenAI Realtime
/// ConversationItem pattern. Access transcribed text via <see cref="Text"/> or
/// <see cref="Transcript"/>.
/// </summary>
public class TranscriptionContentPart
{
    /// <summary>The transcribed text.</summary>
    [JsonPropertyName("text")]
    public string? Text { get; set; }

    /// <summary>The transcript (same as Text for transcription results).</summary>
    [JsonPropertyName("transcript")]
    public string? Transcript { get; set; }
}

/// <summary>
/// Transcription result for real-time audio streaming sessions.
/// Follows the OpenAI Realtime API ConversationItem pattern so that
/// customers access text via <c>result.Content[0].Text</c> or
/// <c>result.Content[0].Transcript</c>.
/// </summary>
public class LiveAudioTranscriptionResponse
{
    /// <summary>
    /// Whether this is a final or partial (interim) result.
    /// - Nemotron models always return <c>true</c> (every result is final).
    /// - Other models (e.g., Azure Embedded) may return <c>false</c> for interim
    ///   hypotheses that will be replaced by a subsequent final result.
    /// </summary>
    [JsonPropertyName("is_final")]
    public bool IsFinal { get; init; }

    /// <summary>Start time offset of this segment in the audio stream (seconds).</summary>
    [JsonPropertyName("start_time")]
    public double? StartTime { get; init; }

    /// <summary>End time offset of this segment in the audio stream (seconds).</summary>
    [JsonPropertyName("end_time")]
    public double? EndTime { get; init; }

    /// <summary>Content parts. Access text via <c>Content[0].Text</c> or <c>Content[0].Transcript</c>.</summary>
    [JsonPropertyName("content")]
    public List<TranscriptionContentPart>? Content { get; set; }

    internal static LiveAudioTranscriptionResponse FromJson(string json)
    {
        var raw = JsonSerializer.Deserialize(json,
            JsonSerializationContext.Default.LiveAudioTranscriptionRaw)
            ?? throw new FoundryLocalException("Failed to deserialize live audio transcription result");

        return new LiveAudioTranscriptionResponse
        {
            IsFinal = raw.IsFinal,
            StartTime = raw.StartTime,
            EndTime = raw.EndTime,
            Content =
            [
                new TranscriptionContentPart
                {
                    Text = raw.Text,
                    Transcript = raw.Text
                }
            ]
        };
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
