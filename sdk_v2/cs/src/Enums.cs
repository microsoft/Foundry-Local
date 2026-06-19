// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

public enum ItemType
{
    Unknown = 0,
    Bytes = 1,
    Tensor = 10,
    Text = 20,
    Message = 21,
    Image = 25,
    Audio = 30,
    ToolCall = 100,
    ToolResult = 101,
    Queue = 200,
}

/// <summary>
/// Subtype tag for <see cref="TextItem"/>. Distinguishes ordinary assistant text from
/// reasoning content (chain-of-thought) and from opaque OpenAI REST JSON payloads
/// carried as text (request/response pass-through).
/// </summary>
public enum TextItemType
{
    Default = 0,
    Reasoning = 1,
    OpenAIJson = 2,  // internal use only
}

public enum MessageRole
{
    None = 0,
    System = 1,
    User = 2,
    Assistant = 3,
    Tool = 4,
    Developer = 5,
}

public enum FinishReason
{
    None = 0,
    Error = 1,
    Stop = 2,
    Length = 3,
    ToolCalls = 4,
}
