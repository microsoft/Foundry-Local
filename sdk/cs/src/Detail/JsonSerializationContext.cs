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
[JsonSerializable(typeof(ChatCompletionRequest))]
[JsonSerializable(typeof(ChatCompletionResponse))]
[JsonSerializable(typeof(ChatChoice))]
[JsonSerializable(typeof(ChatMessage))]
[JsonSerializable(typeof(ChatMessageRole))]
[JsonSerializable(typeof(ToolType))]
[JsonSerializable(typeof(FinishReason))]
[JsonSerializable(typeof(CompletionUsage))]
[JsonSerializable(typeof(ResponseError))]
[JsonSerializable(typeof(AudioTranscriptionRequest))]
[JsonSerializable(typeof(AudioTranscriptionRequestExtended))]
[JsonSerializable(typeof(AudioTranscriptionResponse))]
[JsonSerializable(typeof(string[]))] // list loaded or cached models
[JsonSerializable(typeof(EpInfo[]))]
[JsonSerializable(typeof(EpDownloadResult))]
[JsonSerializable(typeof(JsonElement))]
[JsonSerializable(typeof(ResponseFormat))]
[JsonSerializable(typeof(ResponseFormatExtended))]
[JsonSerializable(typeof(ToolChoice))]
[JsonSerializable(typeof(ToolChoice.FunctionTool))]
[JsonSerializable(typeof(ToolDefinition))]
[JsonSerializable(typeof(IList<ToolDefinition>))]
[JsonSerializable(typeof(FunctionDefinition))]
[JsonSerializable(typeof(IList<FunctionDefinition>))]
[JsonSerializable(typeof(PropertyDefinition))]
[JsonSerializable(typeof(IList<PropertyDefinition>))]
[JsonSerializable(typeof(ToolCall))]
[JsonSerializable(typeof(FunctionCall))]
[JsonSerializable(typeof(JsonSchema))]
[JsonSerializable(typeof(LiveAudioTranscriptionRaw))]
[JsonSerializable(typeof(CoreErrorResponse))]
[JsonSourceGenerationOptions(DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                             WriteIndented = false)]
internal partial class JsonSerializationContext : JsonSerializerContext
{
}
