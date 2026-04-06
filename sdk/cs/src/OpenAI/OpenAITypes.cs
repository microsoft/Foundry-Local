// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json.Serialization;

/// <summary>
/// A chat message in a conversation.
/// </summary>
public class ChatMessage
{
    [JsonPropertyName("role")]
    public string? Role { get; set; }

    [JsonPropertyName("content")]
    public string? Content { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("tool_calls")]
    public List<ToolCall>? ToolCalls { get; set; }

    [JsonPropertyName("tool_call_id")]
    public string? ToolCallId { get; set; }

    public static ChatMessage CreateSystemMessage(string content) =>
        new() { Role = "system", Content = content };

    public static ChatMessage CreateUserMessage(string content) =>
        new() { Role = "user", Content = content };

    public static ChatMessage CreateAssistantMessage(string content) =>
        new() { Role = "assistant", Content = content };

    public static ChatMessage CreateToolMessage(string toolCallId, string content) =>
        new() { Role = "tool", Content = content, ToolCallId = toolCallId };
}

/// <summary>
/// A tool call requested by the assistant.
/// </summary>
public class ToolCall
{
    [JsonPropertyName("id")]
    public string? Id { get; set; }

    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("function")]
    public FunctionCall? FunctionCall { get; set; }
}

/// <summary>
/// A function call within a tool call.
/// </summary>
public class FunctionCall
{
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("arguments")]
    public string? Arguments { get; set; }
}

/// <summary>
/// Request object for creating a chat completion.
/// </summary>
public class ChatCompletionCreateRequest
{
    [JsonPropertyName("model")]
    public string? Model { get; set; }

    [JsonPropertyName("messages")]
    public List<ChatMessage>? Messages { get; set; }

    [JsonPropertyName("tools")]
    public List<ToolDefinition>? Tools { get; set; }

    [JsonPropertyName("stream")]
    public bool? Stream { get; set; }

    [JsonPropertyName("frequency_penalty")]
    public float? FrequencyPenalty { get; set; }

    [JsonPropertyName("max_tokens")]
    public int? MaxTokens { get; set; }

    [JsonPropertyName("n")]
    public int? N { get; set; }

    [JsonPropertyName("temperature")]
    public float? Temperature { get; set; }

    [JsonPropertyName("presence_penalty")]
    public float? PresencePenalty { get; set; }

    [JsonPropertyName("top_p")]
    public float? TopP { get; set; }

    [JsonPropertyName("response_format")]
    public ResponseFormat? ResponseFormat { get; set; }

    [JsonPropertyName("tool_choice")]
    public ToolChoice? ToolChoice { get; set; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }
}

/// <summary>
/// Response from a chat completion request.
/// </summary>
public class ChatCompletionCreateResponse
{
    [JsonPropertyName("id")]
    public string? Id { get; set; }

    [JsonPropertyName("object")]
    public string? ObjectTypeName { get; set; }

    [JsonPropertyName("created")]
    public long? Created { get; set; }

    [JsonPropertyName("model")]
    public string? Model { get; set; }

    [JsonPropertyName("choices")]
    public List<ChatChoice> Choices { get; set; } = [];

    [JsonPropertyName("usage")]
    public CompletionUsage? Usage { get; set; }

    [JsonPropertyName("system_fingerprint")]
    public string? SystemFingerprint { get; set; }
}

/// <summary>
/// A single choice in a chat completion response.
/// </summary>
public class ChatChoice
{
    [JsonPropertyName("index")]
    public int? Index { get; set; }

    [JsonPropertyName("message")]
    public ChatMessage? Message { get; set; }

    [JsonPropertyName("finish_reason")]
    public string? FinishReason { get; set; }

    [JsonPropertyName("logprobs")]
    public object? Logprobs { get; set; }
}

/// <summary>
/// Token usage statistics for a completion request.
/// </summary>
public class CompletionUsage
{
    [JsonPropertyName("prompt_tokens")]
    public int? PromptTokens { get; set; }

    [JsonPropertyName("completion_tokens")]
    public int? CompletionTokens { get; set; }

    [JsonPropertyName("total_tokens")]
    public int? TotalTokens { get; set; }
}

/// <summary>
/// Definition of a tool that can be used by the model.
/// </summary>
public class ToolDefinition
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("function")]
    public FunctionDefinition? Function { get; set; }
}

/// <summary>
/// Definition of a function that can be called as a tool.
/// </summary>
public class FunctionDefinition
{
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("description")]
    public string? Description { get; set; }

    [JsonPropertyName("parameters")]
    public PropertyDefinition? Parameters { get; set; }
}

/// <summary>
/// JSON Schema property definition for function parameters.
/// </summary>
public class PropertyDefinition
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("description")]
    public string? Description { get; set; }

    [JsonPropertyName("properties")]
    public Dictionary<string, PropertyDefinition>? Properties { get; set; }

    [JsonPropertyName("required")]
    public IList<string>? Required { get; set; }

    [JsonPropertyName("enum")]
    public IList<string>? Enum { get; set; }
}

/// <summary>
/// Controls which tool the model should use.
/// </summary>
public class ToolChoice
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    public static ToolChoice None => new() { Type = "none" };
    public static ToolChoice Auto => new() { Type = "auto" };
    public static ToolChoice Required => new() { Type = "required" };
}

/// <summary>
/// Format specification for the model response.
/// </summary>
public class ResponseFormat
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("json_schema")]
    public string? JsonSchema { get; set; }

    [JsonPropertyName("lark_grammar")]
    public string? LarkGrammar { get; set; }
}

/// <summary>
/// Request object for creating an audio transcription.
/// </summary>
public class AudioCreateTranscriptionRequest
{
    [JsonPropertyName("model")]
    public string? Model { get; set; }

    [JsonPropertyName("file_name")]
    public string? FileName { get; set; }

    [JsonPropertyName("language")]
    public string? Language { get; set; }

    [JsonPropertyName("temperature")]
    public float? Temperature { get; set; }

    [JsonPropertyName("metadata")]
    public Dictionary<string, string>? Metadata { get; set; }
}

/// <summary>
/// Response from an audio transcription request.
/// </summary>
public class AudioCreateTranscriptionResponse
{
    [JsonPropertyName("text")]
    public string? Text { get; set; }

    [JsonPropertyName("task")]
    public string? Task { get; set; }

    [JsonPropertyName("language")]
    public string? Language { get; set; }

    [JsonPropertyName("duration")]
    public double? Duration { get; set; }
}

/// <summary>
/// A conversation item in a realtime audio session.
/// </summary>
public class ConversationItem
{
    [JsonPropertyName("id")]
    public string? Id { get; set; }

    [JsonPropertyName("object")]
    public string? Object { get; set; }

    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("status")]
    public string? Status { get; set; }

    [JsonPropertyName("role")]
    public string? Role { get; set; }

    [JsonPropertyName("content")]
    public List<ContentPart>? Content { get; set; }
}

/// <summary>
/// A content part within a conversation item.
/// </summary>
public class ContentPart
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("text")]
    public string? Text { get; set; }

    [JsonPropertyName("transcript")]
    public string? Transcript { get; set; }
}
