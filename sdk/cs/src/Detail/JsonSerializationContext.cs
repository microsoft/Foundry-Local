// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;
using Betalgo.Ranul.OpenAI.ObjectModels.SharedModels;

using Microsoft.AI.Foundry.Local.OpenAI;

[JsonSerializable(typeof(ModelInfo))]
[JsonSerializable(typeof(List<ModelInfo>))]
[JsonSerializable(typeof(CoreInteropRequest))]
[JsonSerializable(typeof(ChatCompletionCreateRequestExtended))]
[JsonSerializable(typeof(ChatCompletionCreateResponse))]
[JsonSerializable(typeof(AudioCreateTranscriptionRequest))]
[JsonSerializable(typeof(AudioCreateTranscriptionResponse))]
[JsonSerializable(typeof(string[]))] // list loaded or cached models
[JsonSerializable(typeof(JsonElement))]
[JsonSerializable(typeof(ResponseFormatExtended))]
[JsonSerializable(typeof(ToolChoice))]
[JsonSerializable(typeof(ToolDefinition))]
[JsonSerializable(typeof(IList<ToolDefinition>))]
[JsonSerializable(typeof(FunctionDefinition))]
[JsonSerializable(typeof(IList<FunctionDefinition>))]
[JsonSerializable(typeof(PropertyDefinition))]
[JsonSerializable(typeof(IList<PropertyDefinition>))]
[JsonSourceGenerationOptions(DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                             WriteIndented = false)]
internal partial class JsonSerializationContext : JsonSerializerContext
{
}
