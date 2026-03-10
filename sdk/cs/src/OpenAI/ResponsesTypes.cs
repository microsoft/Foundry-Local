// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

// ============================================================================
// Responses API Types
// Aligned with OpenAI Responses API / OpenResponses spec.
// ============================================================================

#region Request Types

/// <summary>
/// Request body for POST /v1/responses.
/// </summary>
public class ResponseCreateRequest
{
    [JsonPropertyName("model")]
    public required string Model { get; set; }

    [JsonPropertyName("input")]
    public ResponseInput? Input { get; set; }

    [JsonPropertyName("instructions")]
    public string? Instructions { get; set; }

    [JsonPropertyName("previous_response_id")]
    public string? PreviousResponseId { get; set; }

    [JsonPropertyName("tools")]
    public List<ResponseFunctionTool>? Tools { get; set; }

    [JsonPropertyName("tool_choice")]
    [JsonConverter(typeof(ResponseToolChoiceConverter))]
    public ResponseToolChoice? ToolChoice { get; set; }

    [JsonPropertyName("temperature")]
    public float? Temperature { get; set; }

    [JsonPropertyName("top_p")]
    public float? TopP { get; set; }

    [JsonPropertyName("max_output_tokens")]
    public int? MaxOutputTokens { get; set; }

    [JsonPropertyName("frequency_penalty")]
    public float? FrequencyPenalty { get; set; }

    [JsonPropertyName("presence_penalty")]
    public float? PresencePenalty { get; set; }

    [JsonPropertyName("truncation")]
    public string? Truncation { get; set; }

    [JsonPropertyName("parallel_tool_calls")]
    public bool? ParallelToolCalls { get; set; }

    [JsonPropertyName("store")]
    public bool? Store { get; set; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }

    [JsonPropertyName("stream")]
    public bool? Stream { get; set; }

    [JsonPropertyName("reasoning")]
    public ResponseReasoningConfig? Reasoning { get; set; }

    [JsonPropertyName("text")]
    public ResponseTextConfig? Text { get; set; }

    [JsonPropertyName("seed")]
    public int? Seed { get; set; }

    [JsonPropertyName("user")]
    public string? User { get; set; }
}

/// <summary>
/// Union type for input: either a plain string or an array of items.
/// </summary>
[JsonConverter(typeof(ResponseInputConverter))]
public class ResponseInput
{
    public string? Text { get; set; }
    public List<ResponseItem>? Items { get; set; }

    public static implicit operator ResponseInput(string text) => new() { Text = text };
    public static implicit operator ResponseInput(List<ResponseItem> items) => new() { Items = items };
}

#endregion

#region Response Object

/// <summary>
/// The Response object returned by the Responses API.
/// </summary>
public class ResponseObject
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("object")]
    public string ObjectType { get; set; } = "response";

    [JsonPropertyName("created_at")]
    public long CreatedAt { get; set; }

    [JsonPropertyName("completed_at")]
    public long? CompletedAt { get; set; }

    [JsonPropertyName("failed_at")]
    public long? FailedAt { get; set; }

    [JsonPropertyName("cancelled_at")]
    public long? CancelledAt { get; set; }

    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("incomplete_details")]
    public ResponseIncompleteDetails? IncompleteDetails { get; set; }

    [JsonPropertyName("model")]
    public string Model { get; set; } = string.Empty;

    [JsonPropertyName("previous_response_id")]
    public string? PreviousResponseId { get; set; }

    [JsonPropertyName("instructions")]
    public string? Instructions { get; set; }

    [JsonPropertyName("output")]
    public List<ResponseItem> Output { get; set; } = [];

    [JsonPropertyName("error")]
    public ResponseError? Error { get; set; }

    [JsonPropertyName("tools")]
    public List<ResponseFunctionTool> Tools { get; set; } = [];

    [JsonPropertyName("tool_choice")]
    [JsonConverter(typeof(ResponseToolChoiceConverter))]
    public ResponseToolChoice? ToolChoice { get; set; }

    [JsonPropertyName("truncation")]
    public string? Truncation { get; set; }

    [JsonPropertyName("parallel_tool_calls")]
    public bool ParallelToolCalls { get; set; }

    [JsonPropertyName("text")]
    public ResponseTextConfig? Text { get; set; }

    [JsonPropertyName("top_p")]
    public float TopP { get; set; }

    [JsonPropertyName("temperature")]
    public float Temperature { get; set; }

    [JsonPropertyName("presence_penalty")]
    public float PresencePenalty { get; set; }

    [JsonPropertyName("frequency_penalty")]
    public float FrequencyPenalty { get; set; }

    [JsonPropertyName("max_output_tokens")]
    public int? MaxOutputTokens { get; set; }

    [JsonPropertyName("reasoning")]
    public ResponseReasoningConfig? Reasoning { get; set; }

    [JsonPropertyName("store")]
    public bool Store { get; set; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }

    [JsonPropertyName("usage")]
    public ResponseUsage? Usage { get; set; }

    [JsonPropertyName("user")]
    public string? User { get; set; }

    /// <summary>
    /// Extracts the text from the first assistant message in the output.
    /// Equivalent to OpenAI Python SDK's response.output_text.
    /// </summary>
    [JsonIgnore]
    public string OutputText
    {
        get
        {
            foreach (var item in Output)
            {
                if (item is ResponseMessageItem msg && msg.Role == "assistant")
                {
                    return msg.GetText();
                }
            }
            return string.Empty;
        }
    }
}

#endregion

#region Items (input & output)

/// <summary>
/// Base class for all response items using polymorphic serialization.
/// </summary>
[JsonPolymorphic(TypeDiscriminatorPropertyName = "type")]
[JsonDerivedType(typeof(ResponseMessageItem), "message")]
[JsonDerivedType(typeof(ResponseFunctionCallItem), "function_call")]
[JsonDerivedType(typeof(ResponseFunctionCallOutputItem), "function_call_output")]
public class ResponseItem
{
    [JsonPropertyName("id")]
    public string? Id { get; set; }

    [JsonPropertyName("status")]
    public string? Status { get; set; }
}

public sealed class ResponseMessageItem : ResponseItem
{
    [JsonPropertyName("role")]
    public string Role { get; set; } = string.Empty;

    [JsonPropertyName("content")]
    [JsonConverter(typeof(MessageContentConverter))]
    public ResponseMessageContent Content { get; set; } = new();

    public string GetText()
    {
        if (Content.Text != null)
        {
            return Content.Text;
        }

        if (Content.Parts != null)
        {
            return string.Concat(Content.Parts
                .Where(p => p is ResponseOutputTextContent)
                .Cast<ResponseOutputTextContent>()
                .Select(p => p.Text));
        }

        return string.Empty;
    }
}

public sealed class ResponseFunctionCallItem : ResponseItem
{
    [JsonPropertyName("call_id")]
    public string CallId { get; set; } = string.Empty;

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("arguments")]
    public string Arguments { get; set; } = string.Empty;
}

public sealed class ResponseFunctionCallOutputItem : ResponseItem
{
    [JsonPropertyName("call_id")]
    public string CallId { get; set; } = string.Empty;

    [JsonPropertyName("output")]
    public string Output { get; set; } = string.Empty;
}

#endregion

#region Content Parts

[JsonPolymorphic(TypeDiscriminatorPropertyName = "type")]
[JsonDerivedType(typeof(ResponseInputTextContent), "input_text")]
[JsonDerivedType(typeof(ResponseOutputTextContent), "output_text")]
[JsonDerivedType(typeof(ResponseRefusalContent), "refusal")]
public class ResponseContentPart
{
}

public sealed class ResponseInputTextContent : ResponseContentPart
{
    [JsonPropertyName("text")]
    public string Text { get; set; } = string.Empty;
}

public sealed class ResponseOutputTextContent : ResponseContentPart
{
    [JsonPropertyName("text")]
    public string Text { get; set; } = string.Empty;

    [JsonPropertyName("annotations")]
    public List<JsonElement>? Annotations { get; set; }
}

public sealed class ResponseRefusalContent : ResponseContentPart
{
    [JsonPropertyName("refusal")]
    public string Refusal { get; set; } = string.Empty;
}

/// <summary>
/// Union type for message content: either a plain string or an array of content parts.
/// </summary>
[JsonConverter(typeof(MessageContentConverter))]
public class ResponseMessageContent
{
    public string? Text { get; set; }
    public List<ResponseContentPart>? Parts { get; set; }
}

#endregion

#region Tool Types

public class ResponseFunctionTool
{
    [JsonPropertyName("type")]
    public string Type { get; set; } = "function";

    [JsonPropertyName("name")]
    public required string Name { get; set; }

    [JsonPropertyName("description")]
    public string? Description { get; set; }

    [JsonPropertyName("parameters")]
    public JsonElement? Parameters { get; set; }

    [JsonPropertyName("strict")]
    public bool? Strict { get; set; }
}

/// <summary>
/// Tool choice: either a string value ("none", "auto", "required") or a specific function reference.
/// </summary>
public class ResponseToolChoice
{
    public string? Value { get; set; }
    public ResponseSpecificToolChoice? Specific { get; set; }

    public static implicit operator ResponseToolChoice(string value) => new() { Value = value };

    public static readonly ResponseToolChoice Auto = new() { Value = "auto" };
    public static readonly ResponseToolChoice None = new() { Value = "none" };
    public static readonly ResponseToolChoice Required = new() { Value = "required" };
}

public class ResponseSpecificToolChoice
{
    [JsonPropertyName("type")]
    public string Type { get; set; } = "function";

    [JsonPropertyName("name")]
    public required string Name { get; set; }
}

#endregion

#region Supporting Types

public class ResponseUsage
{
    [JsonPropertyName("input_tokens")]
    public int InputTokens { get; set; }

    [JsonPropertyName("output_tokens")]
    public int OutputTokens { get; set; }

    [JsonPropertyName("total_tokens")]
    public int TotalTokens { get; set; }
}

public class ResponseError
{
    [JsonPropertyName("code")]
    public string Code { get; set; } = string.Empty;

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;
}

public class ResponseIncompleteDetails
{
    [JsonPropertyName("reason")]
    public string Reason { get; set; } = string.Empty;
}

public class ResponseReasoningConfig
{
    [JsonPropertyName("effort")]
    public string? Effort { get; set; }

    [JsonPropertyName("summary")]
    public string? Summary { get; set; }
}

public class ResponseTextConfig
{
    [JsonPropertyName("format")]
    public ResponseTextFormat? Format { get; set; }

    [JsonPropertyName("verbosity")]
    public string? Verbosity { get; set; }
}

public class ResponseTextFormat
{
    [JsonPropertyName("type")]
    public string Type { get; set; } = "text";

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("schema")]
    public JsonElement? Schema { get; set; }

    [JsonPropertyName("strict")]
    public bool? Strict { get; set; }
}

public class ResponseDeleteResult
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("object")]
    public string ObjectType { get; set; } = string.Empty;

    [JsonPropertyName("deleted")]
    public bool Deleted { get; set; }
}

public class ResponseInputItemsList
{
    [JsonPropertyName("object")]
    public string ObjectType { get; set; } = "list";

    [JsonPropertyName("data")]
    public List<ResponseItem> Data { get; set; } = [];
}

#endregion

#region Streaming Events

/// <summary>
/// A streaming event from the Responses API SSE stream.
/// </summary>
public class ResponseStreamingEvent
{
    [JsonPropertyName("type")]
    public string Type { get; set; } = string.Empty;

    [JsonPropertyName("sequence_number")]
    public int SequenceNumber { get; set; }

    // Lifecycle events carry the full response
    [JsonPropertyName("response")]
    public ResponseObject? Response { get; set; }

    // Item events
    [JsonPropertyName("item_id")]
    public string? ItemId { get; set; }

    [JsonPropertyName("output_index")]
    public int? OutputIndex { get; set; }

    [JsonPropertyName("content_index")]
    public int? ContentIndex { get; set; }

    [JsonPropertyName("item")]
    public ResponseItem? Item { get; set; }

    [JsonPropertyName("part")]
    public ResponseContentPart? Part { get; set; }

    // Text delta/done
    [JsonPropertyName("delta")]
    public string? Delta { get; set; }

    [JsonPropertyName("text")]
    public string? Text { get; set; }

    // Function call args
    [JsonPropertyName("arguments")]
    public string? Arguments { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    // Refusal
    [JsonPropertyName("refusal")]
    public string? Refusal { get; set; }

    // Error
    [JsonPropertyName("code")]
    public string? Code { get; set; }

    [JsonPropertyName("message")]
    public string? Message { get; set; }
}

#endregion

#region JSON Converters

internal class ResponseInputConverter : JsonConverter<ResponseInput>
{
    public override ResponseInput? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.String)
        {
            return new ResponseInput { Text = reader.GetString() };
        }

        if (reader.TokenType == JsonTokenType.StartArray)
        {
            var items = JsonSerializer.Deserialize(ref reader, ResponsesJsonContext.Default.ListResponseItem);
            return new ResponseInput { Items = items };
        }

        return null;
    }

    public override void Write(Utf8JsonWriter writer, ResponseInput value, JsonSerializerOptions options)
    {
        if (value.Text != null)
        {
            writer.WriteStringValue(value.Text);
        }
        else if (value.Items != null)
        {
            JsonSerializer.Serialize(writer, value.Items, ResponsesJsonContext.Default.ListResponseItem);
        }
        else
        {
            writer.WriteNullValue();
        }
    }
}

internal class ResponseToolChoiceConverter : JsonConverter<ResponseToolChoice>
{
    public override ResponseToolChoice? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.String)
        {
            return new ResponseToolChoice { Value = reader.GetString() };
        }

        if (reader.TokenType == JsonTokenType.StartObject)
        {
            var specific = JsonSerializer.Deserialize(ref reader, ResponsesJsonContext.Default.ResponseSpecificToolChoice);
            return new ResponseToolChoice { Specific = specific };
        }

        return null;
    }

    public override void Write(Utf8JsonWriter writer, ResponseToolChoice value, JsonSerializerOptions options)
    {
        if (value.Value != null)
        {
            writer.WriteStringValue(value.Value);
        }
        else if (value.Specific != null)
        {
            JsonSerializer.Serialize(writer, value.Specific, ResponsesJsonContext.Default.ResponseSpecificToolChoice);
        }
        else
        {
            writer.WriteNullValue();
        }
    }
}

internal class MessageContentConverter : JsonConverter<ResponseMessageContent>
{
    public override ResponseMessageContent? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.String)
        {
            return new ResponseMessageContent { Text = reader.GetString() };
        }

        if (reader.TokenType == JsonTokenType.StartArray)
        {
            var parts = JsonSerializer.Deserialize(ref reader, ResponsesJsonContext.Default.ListResponseContentPart);
            return new ResponseMessageContent { Parts = parts };
        }

        return null;
    }

    public override void Write(Utf8JsonWriter writer, ResponseMessageContent value, JsonSerializerOptions options)
    {
        if (value.Text != null)
        {
            writer.WriteStringValue(value.Text);
        }
        else if (value.Parts != null)
        {
            JsonSerializer.Serialize(writer, value.Parts, ResponsesJsonContext.Default.ListResponseContentPart);
        }
        else
        {
            writer.WriteNullValue();
        }
    }
}

#endregion
