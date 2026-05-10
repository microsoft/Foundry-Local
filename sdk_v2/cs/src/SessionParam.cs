// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

/// <summary>
/// Well-known parameter keys for <see cref="Session.SetOptions"/> and <see cref="Request.SetOptions"/>.
/// Values are string representations: floats as "0.7", ints as "256", bools as "true"/"false".
/// Arbitrary keys beyond these are also accepted — the implementation passes them through.
/// </summary>
public static class SessionParam
{
    /// <summary>Sampling temperature. Float [0.0, 2.0]. Default is model-specific.</summary>
    public const string Temperature = "temperature";

    /// <summary>Nucleus sampling. Float [0.0, 1.0].</summary>
    public const string TopP = "top_p";

    /// <summary>Top-k sampling. Int.</summary>
    public const string TopK = "top_k";

    /// <summary>Maximum tokens to generate. Int.</summary>
    public const string MaxOutputTokens = "max_output_tokens";

    /// <summary>Frequency penalty. Float [-2.0, 2.0].</summary>
    public const string FrequencyPenalty = "frequency_penalty";

    /// <summary>Presence penalty. Float [-2.0, 2.0].</summary>
    public const string PresencePenalty = "presence_penalty";

    /// <summary>Random seed for reproducible outputs. Int.</summary>
    public const string Seed = "seed";

    /// <summary>Whether to stop on stop sequence or only at max tokens. Bool ("true"/"false").</summary>
    public const string EarlyStopping = "early_stopping";

    /// <summary>Tool choice mode. String: "auto", "none", or "required".</summary>
    public const string ToolChoice = "tool_choice";
}
