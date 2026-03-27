// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI;

using System.Text.Json;
using System.Text.Json.Serialization;

[JsonSerializable(typeof(ResponseCreateRequest))]
[JsonSerializable(typeof(ResponseObject))]
[JsonSerializable(typeof(ResponseDeleteResult))]
[JsonSerializable(typeof(ResponseInputItemsList))]
[JsonSerializable(typeof(ResponseStreamingEvent))]
[JsonSerializable(typeof(ResponseItem))]
[JsonSerializable(typeof(List<ResponseItem>))]
[JsonSerializable(typeof(ResponseMessageItem))]
[JsonSerializable(typeof(ResponseFunctionCallItem))]
[JsonSerializable(typeof(ResponseFunctionCallOutputItem))]
[JsonSerializable(typeof(ResponseContentPart))]
[JsonSerializable(typeof(List<ResponseContentPart>))]
[JsonSerializable(typeof(ResponseInputTextContent))]
[JsonSerializable(typeof(ResponseOutputTextContent))]
[JsonSerializable(typeof(ResponseRefusalContent))]
[JsonSerializable(typeof(ResponseFunctionTool))]
[JsonSerializable(typeof(List<ResponseFunctionTool>))]
[JsonSerializable(typeof(ResponseSpecificToolChoice))]
[JsonSerializable(typeof(ResponseUsage))]
[JsonSerializable(typeof(ResponseError))]
[JsonSerializable(typeof(ResponseIncompleteDetails))]
[JsonSerializable(typeof(ResponseReasoningConfig))]
[JsonSerializable(typeof(ResponseTextConfig))]
[JsonSerializable(typeof(ResponseTextFormat))]
[JsonSerializable(typeof(ResponseMessageContent))]
[JsonSerializable(typeof(ResponseInput))]
[JsonSerializable(typeof(ResponseToolChoice))]
[JsonSerializable(typeof(JsonElement))]
[JsonSourceGenerationOptions(DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
                             WriteIndented = false)]
internal partial class ResponsesJsonContext : JsonSerializerContext
{
}
