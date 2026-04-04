// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json.Serialization;

/// <summary>
/// Response from an audio transcription request.
/// </summary>
public class AudioTranscriptionResponse
{
    /// <summary>The transcribed text.</summary>
    [JsonPropertyName("text")]
    public string? Text { get; set; }

    /// <summary>The task performed (e.g. "transcribe").</summary>
    [JsonPropertyName("task")]
    public string? Task { get; set; }

    /// <summary>The language of the audio.</summary>
    [JsonPropertyName("language")]
    public string? Language { get; set; }

    /// <summary>The duration of the audio in seconds.</summary>
    [JsonPropertyName("duration")]
    public float? Duration { get; set; }
}

/// <summary>
/// Internal request DTO for audio transcription. Most properties use PascalCase
/// (no JsonPropertyName) for native core communication; Metadata is the exception
/// as it follows the OpenAI wire format.
/// </summary>
internal class AudioTranscriptionRequest
{
    public string? Model { get; set; }
    public string? FileName { get; set; }
    public byte[]? File { get; set; }
    public string? Language { get; set; }
    public string? Prompt { get; set; }
    public string? ResponseFormat { get; set; }
    public float? Temperature { get; set; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }
}
