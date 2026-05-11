// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Threading.Channels;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using OpenAIChatMessage = Betalgo.Ranul.OpenAI.ObjectModels.RequestModels.ChatMessage;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

using NativeModel = Microsoft.AI.Foundry.Local.Detail.Native.Model;
using NativeSession = Microsoft.AI.Foundry.Local.Detail.Native.Session;

/// <summary>
/// Chat Client that uses the OpenAI API.
/// Implemented using Betalgo.Ranul.OpenAI SDK types.
/// </summary>
public class OpenAIChatClient
{
    private readonly string _modelId;
    private readonly NativeModel _nativeModel;

    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    internal OpenAIChatClient(string modelId, NativeModel nativeModel)
    {
        _modelId = modelId;
        _nativeModel = nativeModel;
    }

    /// <summary>
    /// Settings that are supported by Foundry Local
    /// </summary>
    public record ChatSettings
    {
        public float? FrequencyPenalty { get; set; }
        public int? MaxTokens { get; set; }
        public int? N { get; set; }
        public float? Temperature { get; set; }
        public float? PresencePenalty { get; set; }
        public int? RandomSeed { get; set; }
        internal bool? Stream { get; set; } // this is set internally based on the API used
        public int? TopK { get; set; }
        public float? TopP { get; set; }
        // Settings for tool calling and structured outputs
        public ResponseFormatExtended? ResponseFormat { get; set; }
        public ToolChoice? ToolChoice { get; set; }
    }

    /// <summary>
    /// Settings to use for chat completions using this client.
    /// </summary>
    public ChatSettings Settings { get; } = new();

    /// <summary>
    /// Execute a chat completion request.
    /// </summary>
    public Task<ChatCompletionCreateResponse> CompleteChatAsync(IEnumerable<OpenAIChatMessage> messages,
                                                                CancellationToken? ct = null)
    {
        return CompleteChatAsync(messages: messages, tools: null, ct: ct);
    }

    /// <summary>
    /// Execute a chat completion request.
    /// </summary>
    public async Task<ChatCompletionCreateResponse> CompleteChatAsync(IEnumerable<OpenAIChatMessage> messages,
                                                                      IEnumerable<ToolDefinition>? tools,
                                                                      CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CompleteChatImplAsync(messages, tools, ct),
            "Error during chat completion.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Execute a chat completion request with streamed output.
    /// </summary>
    public IAsyncEnumerable<ChatCompletionCreateResponse> CompleteChatStreamingAsync(IEnumerable<OpenAIChatMessage> messages,
                                                                                     CancellationToken ct)
    {
        return CompleteChatStreamingAsync(messages: messages, tools: null, ct: ct);
    }

    /// <summary>
    /// Execute a chat completion request with streamed output.
    /// </summary>
    public async IAsyncEnumerable<ChatCompletionCreateResponse> CompleteChatStreamingAsync(IEnumerable<OpenAIChatMessage> messages,
                                                                                           IEnumerable<ToolDefinition>? tools,
                                                                                           [EnumeratorCancellation] CancellationToken ct)
    {
        var enumerable = Utils.CallWithExceptionHandling(
            () => ChatStreamingImplAsync(messages, tools, ct),
            "Error during streaming chat completion.", _logger).ConfigureAwait(false);

        await foreach (var item in enumerable)
        {
            yield return item;
        }
    }

    private async Task<ChatCompletionCreateResponse> CompleteChatImplAsync(IEnumerable<OpenAIChatMessage> messages,
                                                                           IEnumerable<ToolDefinition>? tools,
                                                                           CancellationToken? ct)
    {
        Settings.Stream = false;

        var chatRequest = ChatCompletionCreateRequestExtended.FromUserInput(_modelId, messages, tools, Settings);
        var chatRequestJson = chatRequest.ToJson();

        return await Task.Run(() =>
        {
            using var session = new NativeSession(_nativeModel);
            using var jsonItem = new TextItem(chatRequestJson, TextItemType.OpenAIJson);
            using var request = new Request();
            request.AddItem(jsonItem);

            var responsePtr = session.ProcessRequest(request.Ptr);
            using var response = new Response(responsePtr);

            // Get the response JSON from the first item
            using var responseItem = response.GetItem(0);
            var responseJson = ((TextItem)responseItem).Text;

            return JsonSerializer.Deserialize(responseJson,
                       JsonSerializationContext.Default.ChatCompletionCreateResponse)
                   ?? throw new FoundryLocalException("Failed to deserialize chat completion response.");
        }).ConfigureAwait(false);
    }

    private async IAsyncEnumerable<ChatCompletionCreateResponse> ChatStreamingImplAsync(
        IEnumerable<OpenAIChatMessage> messages,
        IEnumerable<ToolDefinition>? tools,
        [EnumeratorCancellation] CancellationToken ct)
    {
        Settings.Stream = true;

        var chatRequest = ChatCompletionCreateRequestExtended.FromUserInput(_modelId, messages, tools, Settings);
        var chatRequestJson = chatRequest.ToJson();

        var channel = Channel.CreateUnbounded<ChatCompletionCreateResponse>(
                        new UnboundedChannelOptions
                        {
                            SingleWriter = true,
                            SingleReader = true,
                            AllowSynchronousContinuations = true
                        });

        _ = Task.Run(() =>
        {
            try
            {
                using var session = new NativeSession(_nativeModel);
                FlStreamingCallback streamingCallback = (FlStreamingCallbackData data, IntPtr userData) =>
                {
                    try
                    {
                        // data.ItemQueue is the native queue pointer
                        if (data.ItemQueue != IntPtr.Zero)
                        {
                            while (Api.Item.QueueTryPop(data.ItemQueue, out var itemPtr))
                            {
                                using var item = Item.FromNative(itemPtr, ownsHandle: true);
                                var responseJson = ((TextItem)item).Text;

                                var chunk = JsonSerializer.Deserialize(responseJson,
                                    JsonSerializationContext.Default.ChatCompletionCreateResponse);

                                if (chunk != null)
                                {
                                    channel.Writer.TryWrite(chunk);
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        channel.Writer.TryComplete(
                            new FoundryLocalException("Error processing streaming chat completion callback data.", ex, _logger));
                    }

                    return ct.IsCancellationRequested ? 1 : 0;
                };

                session.SetStreamingCallback(streamingCallback);

                using var jsonItem = new TextItem(chatRequestJson, TextItemType.OpenAIJson);
                using var request = new Request();
                request.AddItem(jsonItem);

                var responsePtr = session.ProcessRequest(request.Ptr);
                Api.Inference.ResponseRelease(responsePtr);

                // Streaming is done when ProcessRequest returns
                channel.Writer.TryComplete();
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error executing streaming chat completion.", ex, _logger));
            }
            catch (OperationCanceledException)
            {
                channel.Writer.TryComplete();
            }
        }, ct);

        await foreach (var item in channel.Reader.ReadAllAsync(ct))
        {
            yield return item;
        }
    }
}
