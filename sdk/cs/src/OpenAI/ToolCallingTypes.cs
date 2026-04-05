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
    /// <summary>The type of tool. Defaults to <see cref="ToolType.Function"/>.</summary>
    [JsonPropertyName("type")]
    public ToolType Type { get; set; } = ToolType.Function;

    /// <summary>The function definition.</summary>
    [JsonPropertyName("function")]
    public required FunctionDefinition Function { get; set; }
}

/// <summary>
/// Definition of a function the model may call.
/// </summary>
public class FunctionDefinition
{
    /// <summary>The name of the function.</summary>
    [JsonPropertyName("name")]
    public required string Name { get; set; }

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
    /// <summary>
    /// The data type. Can be a single type string (e.g. "object", "string", "integer")
    /// or an array of types (e.g. ["string", "null"]) per JSON Schema specification.
    /// </summary>
    [JsonPropertyName("type")]
    [JsonConverter(typeof(JsonSchemaTypeConverter))]
    public object? Type { get; set; }

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
/// Use static methods <see cref="CreateNoneChoice"/>, <see cref="CreateAutoChoice"/>,
/// <see cref="CreateRequiredChoice"/>, or <see cref="CreateFunctionChoice(string)"/>.
/// </summary>
[JsonConverter(typeof(ToolChoiceConverter))]
public class ToolChoice
{
    /// <summary>The tool choice type.</summary>
    public string? Type { get; internal set; }

    /// <summary>Specifies a particular function to call.</summary>
    public FunctionTool? Function { get; internal set; }

    /// <summary>Creates a choice indicating the model will not call any tool.</summary>
    public static ToolChoice CreateNoneChoice() => new() { Type = "none" };

    /// <summary>Creates a choice indicating the model can choose whether to call a tool.</summary>
    public static ToolChoice CreateAutoChoice() => new() { Type = "auto" };

    /// <summary>Creates a choice indicating the model must call one or more tools.</summary>
    public static ToolChoice CreateRequiredChoice() => new() { Type = "required" };

    /// <summary>Creates a choice indicating the model must call the specified function.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="functionName"/> is null.</exception>
    /// <exception cref="ArgumentException"><paramref name="functionName"/> is empty.</exception>
    public static ToolChoice CreateFunctionChoice(string functionName)
    {
        ArgumentNullException.ThrowIfNullOrEmpty(functionName, nameof(functionName));
        return new() { Type = "function", Function = new FunctionTool { Name = functionName } };
    }

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
        if (reader.TokenType == JsonTokenType.Null)
        {
            return null;
        }

        if (reader.TokenType == JsonTokenType.String)
        {
            return new ToolChoice { Type = reader.GetString() };
        }

        if (reader.TokenType != JsonTokenType.StartObject)
        {
            throw new JsonException($"Unexpected token type {reader.TokenType} when deserializing ToolChoice.");
        }

        var choice = new ToolChoice();
        while (reader.Read() && reader.TokenType != JsonTokenType.EndObject)
        {
            if (reader.TokenType != JsonTokenType.PropertyName)
            {
                continue;
            }

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
            if (value.Type == null)
            {
                throw new JsonException("ToolChoice.Type must not be null when serializing.");
            }
            writer.WriteStringValue(value.Type);
            return;
        }

        writer.WriteStartObject();
        writer.WriteString("type", value.Type ?? "function");
        writer.WritePropertyName("function");
        JsonSerializer.Serialize(writer, value.Function, JsonSerializationContext.Default.FunctionTool);
        writer.WriteEndObject();
    }
}

/// <summary>
/// Custom JSON converter for the <see cref="PropertyDefinition.Type"/> property that handles
/// both single type strings (<c>"string"</c>) and type arrays (<c>["string", "null"]</c>)
/// per JSON Schema specification.
/// </summary>
internal class JsonSchemaTypeConverter : JsonConverter<object?>
{
    public override object? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.Null)
        {
            return null;
        }

        if (reader.TokenType == JsonTokenType.String)
        {
            return reader.GetString();
        }

        if (reader.TokenType == JsonTokenType.StartArray)
        {
            var list = new List<string>();
            while (reader.Read() && reader.TokenType != JsonTokenType.EndArray)
            {
                if (reader.TokenType == JsonTokenType.String)
                {
                    list.Add(reader.GetString()!);
                }
                else
                {
                    throw new JsonException($"Expected string in type array, got {reader.TokenType}.");
                }
            }
            return list;
        }

        throw new JsonException($"Unexpected token type {reader.TokenType} for JSON Schema 'type'.");
    }

    public override void Write(Utf8JsonWriter writer, object? value, JsonSerializerOptions options)
    {
        switch (value)
        {
            case null:
                writer.WriteNullValue();
                break;
            case string s:
                writer.WriteStringValue(s);
                break;
            case IEnumerable<string> arr:
                writer.WriteStartArray();
                foreach (var item in arr)
                {
                    writer.WriteStringValue(item);
                }
                writer.WriteEndArray();
                break;
            default:
                throw new JsonException($"Cannot serialize {value.GetType()} as JSON Schema 'type'.");
        }
    }
}
