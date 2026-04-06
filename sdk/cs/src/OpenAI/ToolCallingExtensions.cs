// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json.Serialization;

/// <summary>
/// Extended response format that adds LARK grammar support beyond the OpenAI spec.
/// </summary>
/// <remarks>
/// Supported formats:
/// <list type="number">
///   <item><c>{"type": "text"}</c></item>
///   <item><c>{"type": "json_object"}</c></item>
///   <item><c>{"type": "json_schema", "json_schema": ...}</c></item>
///   <item><c>{"type": "lark_grammar", "lark_grammar": ...}</c></item>
/// </list>
/// </remarks>
public class ResponseFormatExtended : ResponseFormat
{
    /// <summary>LARK grammar string when type is "lark_grammar".</summary>
    [JsonPropertyName("lark_grammar")]
    public string? LarkGrammar { get; set; }
}
