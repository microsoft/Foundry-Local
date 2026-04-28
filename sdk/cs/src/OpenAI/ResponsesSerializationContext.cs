// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.OpenAI.Responses;

using System.Collections.Generic;
using System.Text.Json.Serialization;

/// <summary>
/// Source-generated JSON serialization context for the Responses API types.
/// This keeps the SDK AOT-compatible and trimming-safe.
/// </summary>
[JsonSourceGenerationOptions(
    WriteIndented = false,
    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    PropertyNameCaseInsensitive = true,
    UseStringEnumConverter = true)]
[JsonSerializable(typeof(ResponseCreateRequest))]
[JsonSerializable(typeof(ResponseObject))]
[JsonSerializable(typeof(StreamingEvent))]
[JsonSerializable(typeof(ResponseItem))]
[JsonSerializable(typeof(ContentPart))]
[JsonSerializable(typeof(Annotation))]
[JsonSerializable(typeof(DeleteResponseResult))]
[JsonSerializable(typeof(InputItemsListResponse))]
[JsonSerializable(typeof(ListResponsesResult))]
[JsonSerializable(typeof(MessageItem))]
[JsonSerializable(typeof(FunctionCallItem))]
[JsonSerializable(typeof(FunctionCallOutputItem))]
[JsonSerializable(typeof(ReasoningItem))]
[JsonSerializable(typeof(ItemReference))]
[JsonSerializable(typeof(InputTextContent))]
[JsonSerializable(typeof(OutputTextContent))]
[JsonSerializable(typeof(RefusalContent))]
[JsonSerializable(typeof(InputImageContent))]
[JsonSerializable(typeof(InputFileContent))]
[JsonSerializable(typeof(UrlCitationAnnotation))]
[JsonSerializable(typeof(FunctionToolDefinition))]
[JsonSerializable(typeof(SpecificToolChoice))]
[JsonSerializable(typeof(ToolChoice))]
[JsonSerializable(typeof(ReasoningConfig))]
[JsonSerializable(typeof(TextConfig))]
[JsonSerializable(typeof(TextFormat))]
[JsonSerializable(typeof(ResponseUsage))]
[JsonSerializable(typeof(ResponseError))]
[JsonSerializable(typeof(IncompleteDetails))]
[JsonSerializable(typeof(ApiErrorResponse))]
[JsonSerializable(typeof(ResponseCreatedEvent))]
[JsonSerializable(typeof(ResponseQueuedEvent))]
[JsonSerializable(typeof(ResponseInProgressEvent))]
[JsonSerializable(typeof(ResponseCompletedEvent))]
[JsonSerializable(typeof(ResponseFailedEvent))]
[JsonSerializable(typeof(ResponseIncompleteEvent))]
[JsonSerializable(typeof(OutputItemAddedEvent))]
[JsonSerializable(typeof(OutputItemDoneEvent))]
[JsonSerializable(typeof(ContentPartAddedEvent))]
[JsonSerializable(typeof(ContentPartDoneEvent))]
[JsonSerializable(typeof(OutputTextDeltaEvent))]
[JsonSerializable(typeof(OutputTextDoneEvent))]
[JsonSerializable(typeof(RefusalDeltaEvent))]
[JsonSerializable(typeof(RefusalDoneEvent))]
[JsonSerializable(typeof(FunctionCallArgumentsDeltaEvent))]
[JsonSerializable(typeof(FunctionCallArgumentsDoneEvent))]
[JsonSerializable(typeof(ReasoningSummaryPartAddedEvent))]
[JsonSerializable(typeof(ReasoningSummaryPartDoneEvent))]
[JsonSerializable(typeof(ReasoningDeltaEvent))]
[JsonSerializable(typeof(ReasoningDoneEvent))]
[JsonSerializable(typeof(ReasoningSummaryDeltaEvent))]
[JsonSerializable(typeof(ReasoningSummaryDoneEvent))]
[JsonSerializable(typeof(OutputTextAnnotationAddedEvent))]
[JsonSerializable(typeof(ErrorEvent))]
[JsonSerializable(typeof(List<ContentPart>))]
[JsonSerializable(typeof(List<ResponseItem>))]
[JsonSerializable(typeof(List<StreamingEvent>))]
[JsonSerializable(typeof(List<FunctionToolDefinition>))]
[JsonSerializable(typeof(Dictionary<string, string>))]
internal partial class ResponsesSerializationContext : JsonSerializerContext
{
}
