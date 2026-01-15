// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading.Channels;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using Betalgo.Ranul.OpenAI.ObjectModels.ResponseModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;

/// <summary>
/// Chat Client that uses the OpenAI API.
/// Implemented using Betalgo.Ranul.OpenAI SDK types.
/// </summary>
public class OpenAIChatClient
{
    private readonly string _modelId;

    private readonly ICoreInterop _coreInterop = FoundryLocalManager.Instance.CoreInterop;
    private readonly ILogger _logger = FoundryLocalManager.Instance.Logger;

    internal OpenAIChatClient(string modelId)
    {
        _modelId = modelId;
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
    }

    /// <summary>
    /// Settings to use for chat completions using this client.
    /// </summary>
    public ChatSettings Settings { get; } = new();

    /// <summary>
    /// Execute a chat completion request.
    ///
    /// To continue a conversation, add the ChatMessage from the previous response and new prompt to the messages.
    /// </summary>
    /// <param name="messages">Chat messages. The system message is automatically added.</param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Chat completion response.</returns>
    public async Task<ChatCompletionCreateResponse> CompleteChatAsync(IEnumerable<ChatMessage> messages,
                                                                      CancellationToken? ct = null)
    {
        return await Utils.CallWithExceptionHandling(
            () => CompleteChatImplAsync(messages, ct),
            "Error during chat completion.", _logger).ConfigureAwait(false);
    }

    /// <summary>
    /// Execute a chat completion request with streamed output.
    ///
    /// To continue a conversation, add the ChatMessage from the previous response and new prompt to the messages.
    /// </summary>
    /// <param name="messages">Chat messages. The system message is automatically added.</param>
    /// <param name="ct">Optional cancellation token.</param>
    /// <returns>Async enumerable of chat completion responses.</returns>
    public async IAsyncEnumerable<ChatCompletionCreateResponse> CompleteChatStreamingAsync(
        IEnumerable<ChatMessage> messages, [EnumeratorCancellation] CancellationToken ct)
    {
        var enumerable = Utils.CallWithExceptionHandling(
            () => ChatStreamingImplAsync(messages, ct),
            "Error during streaming chat completion.", _logger).ConfigureAwait(false);

        await foreach (var item in enumerable)
        {
            yield return item;
        }
    }

    private async Task<ChatCompletionCreateResponse> CompleteChatImplAsync(IEnumerable<ChatMessage> messages,
                                                                           CancellationToken? ct)
    {
        Settings.Stream = false;

        var chatRequest = ChatCompletionCreateRequestExtended.FromUserInput(_modelId, messages, Settings);
        var chatRequestJson = chatRequest.ToJson();

        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", chatRequestJson } } };
        var response = await _coreInterop.ExecuteCommandAsync("chat_completions", request,
                                                                ct ?? CancellationToken.None).ConfigureAwait(false);

        var chatCompletion = response.ToChatCompletion(_logger);

        return chatCompletion;
    }

    private async IAsyncEnumerable<ChatCompletionCreateResponse> ChatStreamingImplAsync(
        IEnumerable<ChatMessage> messages, [EnumeratorCancellation] CancellationToken ct)
    {
        Settings.Stream = true;

        var chatRequest = ChatCompletionCreateRequestExtended.FromUserInput(_modelId, messages, Settings);
        var chatRequestJson = chatRequest.ToJson();
        var request = new CoreInteropRequest { Params = new() { { "OpenAICreateRequest", chatRequestJson } } };

        var channel = Channel.CreateUnbounded<ChatCompletionCreateResponse>(
                        new UnboundedChannelOptions
                        {
                            SingleWriter = true,
                            SingleReader = true,
                            AllowSynchronousContinuations = true
                        });

        // The callback will push ChatResponse objects into the channel.
        // The channel reader will return the values to the user.
        // This setup prevents the user from blocking the thread generating the responses.
        _ = Task.Run(async () =>
        {
            try
            {
                var failed = false;

                await _coreInterop.ExecuteCommandWithCallbackAsync(
                    "chat_completions",
                    request,
                    async (callbackData) =>
                    {
                        try
                        {
                            if (!failed)
                            {
                                var chatCompletion = callbackData.ToChatCompletion(_logger);
                                await channel.Writer.WriteAsync(chatCompletion);
                            }
                        }
                        catch (Exception ex)
                        {
                            // propagate exception to reader
                            channel.Writer.TryComplete(
                                new FoundryLocalException("Error processing streaming chat completion callback data.",
                                                          ex, _logger));
                            failed = true;
                        }
                    },
                    ct
                ).ConfigureAwait(false);

                // use TryComplete as an exception in the callback may have already closed the channel
                _ = channel.Writer.TryComplete();
            }
            // Ignore cancellation exceptions so we don't convert them into errors
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(
                    new FoundryLocalException("Error executing streaming chat completion.", ex, _logger));
            }
            catch (OperationCanceledException)
            {
                // Complete the channel on cancellation but don't turn it into an error
                channel.Writer.TryComplete();
            }
        }, ct);

        // Start reading from the channel as items arrive.
        // This will continue until ExecuteCommandWithCallbackAsync completes and closes the channel.
        await foreach (var item in channel.Reader.ReadAllAsync(ct))
        {
            yield return item;
        }
    }
}
