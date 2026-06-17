// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// Token usage statistics from an inference response.
/// </summary>
public sealed class TokenUsage
{
    public int PromptTokens { get; }
    public int CompletionTokens { get; }
    public int TotalTokens { get; }

    internal TokenUsage(int promptTokens, int completionTokens, int totalTokens)
    {
        PromptTokens = promptTokens;
        CompletionTokens = completionTokens;
        TotalTokens = totalTokens;
    }
}
