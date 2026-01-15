// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;
using System.Collections.Generic;
using System.Text.Json.Serialization;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.OpenAI;

[JsonSerializable(typeof(ModelInfo))]
[JsonSerializable(typeof(List<ModelInfo>))]
[JsonSerializable(typeof(CoreInteropRequest))]
[JsonSerializable(typeof(ChatCompletionCreateRequestExtended))]
[JsonSerializable(typeof(ChatCompletionCreateResponse))]
[JsonSerializable(typeof(AudioCreateTranscriptionRequest))]
[JsonSerializable(typeof(AudioCreateTranscriptionResponse))]
[JsonSerializable(typeof(string[]))] // list loaded or cached models
[JsonSourceGenerationOptions(DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                             WriteIndented = false)]
internal partial class JsonSerializationContext : JsonSerializerContext
{
}
