// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// Convenience wrapper for an OpenAI REST JSON payload (chat completions or audio transcription
/// request/response). Internally a <see cref="TextItem"/> with <see cref="TextItemType.OpenAIJson"/>.
/// </summary>
public sealed class JsonItem : TextItem
{
    public string Json => Text;

    public JsonItem(string json) : base(json, TextItemType.OpenAIJson)
    {
    }

    internal JsonItem(IntPtr ptr, bool ownsHandle) : base(ptr, ownsHandle)
    {
    }
}
