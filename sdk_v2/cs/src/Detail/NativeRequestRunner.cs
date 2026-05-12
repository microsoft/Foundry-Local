// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Detail;

using System.Runtime.CompilerServices;
using System.Threading.Channels;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.Extensions.Logging;

using NativeApi = Microsoft.AI.Foundry.Local.Detail.Native.Api;
using NativeModel = Microsoft.AI.Foundry.Local.Detail.Native.Model;
using NativeSession = Microsoft.AI.Foundry.Local.Detail.Native.Session;

/// <summary>
/// Shared choreography for OpenAI-style "send one JSON request, get one (or many streaming) JSON
/// response items" calls into the native SDK. Encapsulates the duplicated session/request/response
/// + producer/consumer/channel/cancellation pattern so each new client only supplies request JSON
/// and a deserializer.
/// </summary>
internal static class NativeRequestRunner
{
    /// <summary>
    /// Run a single (non-streaming) request on the thread pool and deserialize the first response
    /// item via <paramref name="deserialize"/>.
    /// </summary>
    internal static Task<T> RunAsync<T>(NativeModel nativeModel,
                                        string requestJson,
                                        Func<string, T> deserialize,
                                        CancellationToken? ct)
    {
        return Task.Run(() =>
        {
            using var session = new NativeSession(nativeModel);
            using var jsonItem = new TextItem(requestJson, TextItemType.OpenAIJson);
            using var request = new Request();
            request.AddItem(jsonItem);

            var responsePtr = session.ProcessRequest(request.Ptr);
            using var response = new Response(responsePtr);

            using var responseItem = response.GetItem(0);
            var responseJson = ((TextItem)responseItem).Text;

            return deserialize(responseJson);
        }, ct ?? CancellationToken.None);
    }

    /// <summary>
    /// Run a streaming request. Each native callback invocation is drained into an unbounded channel
    /// and surfaced to the consumer. The producer task is awaited in <c>finally</c> so disposing the
    /// enumerator early cancels and joins the producer cleanly. <paramref name="deserialize"/> may
    /// return <c>null</c> to skip a chunk.
    /// </summary>
    internal static async IAsyncEnumerable<T> RunStreamingAsync<T>(NativeModel nativeModel,
                                                                   string requestJson,
                                                                   Func<string, T?> deserialize,
                                                                   ILogger logger,
                                                                   string callbackErrorMsg,
                                                                   string runErrorMsg,
                                                                   [EnumeratorCancellation] CancellationToken ct)
        where T : class
    {
        var channel = Channel.CreateUnbounded<T>(new UnboundedChannelOptions
        {
            SingleWriter = true,
            SingleReader = true,
            AllowSynchronousContinuations = true
        });

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        var stopToken = cts.Token;

        var producerTask = Task.Run(() =>
        {
            try
            {
                using var session = new NativeSession(nativeModel);

                FlStreamingCallback streamingCallback = (FlStreamingCallbackData data, IntPtr userData) =>
                {
                    try
                    {
                        if (data.ItemQueue != IntPtr.Zero)
                        {
                            while (NativeApi.Item.QueueTryPop(data.ItemQueue, out var itemPtr))
                            {
                                using var item = Item.FromNative(itemPtr, ownsHandle: true);
                                var responseJson = ((TextItem)item).Text;

                                var chunk = deserialize(responseJson);

                                if (chunk != null)
                                {
                                    channel.Writer.TryWrite(chunk);
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        channel.Writer.TryComplete(new FoundryLocalException(callbackErrorMsg, ex, logger));
                    }

                    return stopToken.IsCancellationRequested ? 1 : 0;
                };

                session.SetStreamingCallback(streamingCallback);

                using var jsonItem = new TextItem(requestJson, TextItemType.OpenAIJson);
                using var request = new Request();
                request.AddItem(jsonItem);

                var responsePtr = session.ProcessRequest(request.Ptr);
                NativeApi.Inference.ResponseRelease(responsePtr);

                channel.Writer.TryComplete();
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                channel.Writer.TryComplete(new FoundryLocalException(runErrorMsg, ex, logger));
            }
            catch (OperationCanceledException)
            {
                channel.Writer.TryComplete();
            }
        }, CancellationToken.None);

        try
        {
            await foreach (var item in channel.Reader.ReadAllAsync(stopToken).ConfigureAwait(false))
            {
                yield return item;
            }
        }
        finally
        {
            try { cts.Cancel(); } catch { }

            try
            {
                await producerTask.ConfigureAwait(false);
            }
            catch
            {
                // Producer exceptions are routed via channel completion.
            }
        }
    }
}
