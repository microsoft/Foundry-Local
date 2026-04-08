// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

using Microsoft.AI.Foundry.Local.OpenAI;

[JsonSerializable(typeof(ModelInfo))]
[JsonSerializable(typeof(List<ModelInfo>))]
[JsonSerializable(typeof(CoreInteropRequest))]
[JsonSerializable(typeof(ChatCompletionCreateRequest))]
[JsonSerializable(typeof(ChatCompletionCreateResponse))]
[JsonSerializable(typeof(AudioCreateTranscriptionRequest))]
[JsonSerializable(typeof(AudioCreateTranscriptionResponse))]
[JsonSerializable(typeof(string[]))] // list loaded or cached models
[JsonSerializable(typeof(EpInfo[]))]
[JsonSerializable(typeof(EpDownloadResult))]
[JsonSerializable(typeof(JsonElement))]
[JsonSerializable(typeof(ResponseFormat))]
[JsonSerializable(typeof(ToolChoice))]
[JsonSerializable(typeof(ToolDefinition))]
[JsonSerializable(typeof(IList<ToolDefinition>))]
[JsonSerializable(typeof(FunctionDefinition))]
[JsonSerializable(typeof(IList<FunctionDefinition>))]
[JsonSerializable(typeof(PropertyDefinition))]
[JsonSerializable(typeof(IList<PropertyDefinition>))]
// --- Audio streaming types (we only register the raw deserialization type used by LiveAudioTranscriptionResponse.FromJson) ---
[JsonSerializable(typeof(LiveAudioTranscriptionRaw))]
[JsonSerializable(typeof(CoreErrorResponse))]
[JsonSourceGenerationOptions(DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                             WriteIndented = false)]
internal partial class JsonSerializationContext : JsonSerializerContext
{
}
