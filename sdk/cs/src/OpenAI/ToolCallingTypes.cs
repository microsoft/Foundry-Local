// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

using Microsoft.AI.Foundry.Local.Detail;

/// <summary>
/// Definition of a tool the model may call.
/// </summary>
public class ToolDefinition
{
    /// <summary>The type of tool (e.g. "function").</summary>
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    /// <summary>The function definition.</summary>
    [JsonPropertyName("function")]
    public FunctionDefinition? Function { get; set; }
}

/// <summary>
/// Definition of a function the model may call.
/// </summary>
public class FunctionDefinition
{
    /// <summary>The name of the function.</summary>
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    /// <summary>A description of what the function does.</summary>
    [JsonPropertyName("description")]
    public string? Description { get; set; }

    /// <summary>The parameters the function accepts, described as a JSON Schema object.</summary>
    [JsonPropertyName("parameters")]
    public PropertyDefinition? Parameters { get; set; }

    /// <summary>Whether to enable strict schema adherence.</summary>
    [JsonPropertyName("strict")]
    public bool? Strict { get; set; }
}

/// <summary>
/// JSON Schema property definition used to describe function parameters and structured outputs.
/// </summary>
public class PropertyDefinition
{
    /// <summary>The data type (e.g. "object", "string", "integer", "array").</summary>
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    /// <summary>A description of the property.</summary>
    [JsonPropertyName("description")]
    public string? Description { get; set; }

    /// <summary>Allowed values for enum types.</summary>
    [JsonPropertyName("enum")]
    public IList<string>? Enum { get; set; }

    /// <summary>Nested properties for object types.</summary>
    [JsonPropertyName("properties")]
    public IDictionary<string, PropertyDefinition>? Properties { get; set; }

    /// <summary>Required property names for object types.</summary>
    [JsonPropertyName("required")]
    public IList<string>? Required { get; set; }

    /// <summary>Schema for array item types.</summary>
    [JsonPropertyName("items")]
    public PropertyDefinition? Items { get; set; }

    /// <summary>Whether additional properties are allowed.</summary>
    [JsonPropertyName("additionalProperties")]
    public bool? AdditionalProperties { get; set; }
}

/// <summary>
/// Response format specification for chat completions.
/// </summary>
public class ResponseFormat
{
    /// <summary>The format type (e.g. "text", "json_object", "json_schema").</summary>
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    /// <summary>JSON Schema specification when type is "json_schema".</summary>
    [JsonPropertyName("json_schema")]
    public JsonSchema? JsonSchema { get; set; }
}

/// <summary>
/// JSON Schema definition for structured output.
/// </summary>
public class JsonSchema
{
    /// <summary>The name of the schema.</summary>
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    /// <summary>Whether to enable strict schema adherence.</summary>
    [JsonPropertyName("strict")]
    public bool? Strict { get; set; }

    /// <summary>The JSON Schema definition.</summary>
    [JsonPropertyName("schema")]
    public PropertyDefinition? Schema { get; set; }
}

/// <summary>
/// Controls which tool the model should use.
/// Use static properties <see cref="None"/>, <see cref="Auto"/>, or <see cref="Required"/>
/// for standard choices.
/// </summary>
[JsonConverter(typeof(ToolChoiceConverter))]
public class ToolChoice
{
    /// <summary>The tool choice type.</summary>
    public string? Type { get; set; }

    /// <summary>Specifies a particular function to call.</summary>
    public FunctionTool? Function { get; set; }

    /// <summary>The model will not call any tool.</summary>
    public static ToolChoice None => new() { Type = "none" };

    /// <summary>The model can choose whether to call a tool.</summary>
    public static ToolChoice Auto => new() { Type = "auto" };

    /// <summary>The model must call one or more tools.</summary>
    public static ToolChoice Required => new() { Type = "required" };

    /// <summary>
    /// Specifies a specific function tool to call.
    /// </summary>
    public class FunctionTool
    {
        /// <summary>The name of the function to call.</summary>
        [JsonPropertyName("name")]
        public string? Name { get; set; }
    }
}

/// <summary>
/// Custom JSON converter for <see cref="ToolChoice"/> that serializes
/// simple choices ("none", "auto", "required") as plain strings
/// and specific function choices as objects.
/// </summary>
internal class ToolChoiceConverter : JsonConverter<ToolChoice>
{
    public override ToolChoice? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.String)
            return new ToolChoice { Type = reader.GetString() };

        if (reader.TokenType != JsonTokenType.StartObject)
            return null;

        var choice = new ToolChoice();
        while (reader.Read() && reader.TokenType != JsonTokenType.EndObject)
        {
            if (reader.TokenType != JsonTokenType.PropertyName) continue;
            var prop = reader.GetString();
            reader.Read();
            switch (prop)
            {
                case "type":
                    choice.Type = reader.GetString();
                    break;
                case "function":
                    choice.Function = JsonSerializer.Deserialize(ref reader, JsonSerializationContext.Default.FunctionTool);
                    break;
                default:
                    reader.Skip();
                    break;
            }
        }
        return choice;
    }

    public override void Write(Utf8JsonWriter writer, ToolChoice value, JsonSerializerOptions options)
    {
        if (value.Function == null)
        {
            writer.WriteStringValue(value.Type);
            return;
        }

        writer.WriteStartObject();
        writer.WriteString("type", value.Type);
        writer.WritePropertyName("function");
        JsonSerializer.Serialize(writer, value.Function, JsonSerializationContext.Default.FunctionTool);
        writer.WriteEndObject();
    }
}
