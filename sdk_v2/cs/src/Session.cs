// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading.Channels;

using Microsoft.AI.Foundry.Local.Detail.Interop;
using Microsoft.AI.Foundry.Local.Detail.Native;

using NativeSession = Microsoft.AI.Foundry.Local.Detail.Native.Session;

/// <summary>
/// Base session wrapping the native inference session.
/// Provides request processing, streaming, options, and tool definitions.
/// Use <see cref="ChatSession"/> or <see cref="AudioSession"/> for task-specific validation.
/// </summary>
public abstract class Session : IDisposable
{
    private readonly NativeSession _session;
    private FlStreamingCallback? _nativeStreamingCallback;
    private Channel<Item>? _activeChannel;
    private CancellationToken _streamingCt;
    private bool _disposed;

    /// <summary>
    /// Create a session from a loaded model. Subclasses should validate the model task before calling this.
    /// </summary>
    protected Session(IModel model)
    {
        var concrete = (Model)model;
        _session = new NativeSession(concrete.NativeModel);
    }

    /// <summary>
    /// Set session-level inference options. These apply to all subsequent
    /// <see cref="ProcessRequestAsync"/> calls unless overridden per-request.
    /// Use <see cref="SessionParam"/> constants for well-known keys.
    /// Values are string representations (e.g. "0.7" for floats, "256" for ints, "true"/"false" for bools).
    /// Arbitrary keys beyond the well-known set are passed through to the native layer.
    /// </summary>
    /// <returns>This session (fluent).</returns>
    public Session SetOptions(IDictionary<string, string> options)
    {
        ThrowIfDisposed();

        Api.Root.CreateKeyValuePairs(out var kvpPtr);

        try
        {
            foreach (var kvp in options)
            {
                Api.Root.AddKeyValuePair(kvpPtr, kvp.Key, kvp.Value);
            }

            _session.SetOptions(kvpPtr);
        }
        finally
        {
            Api.Root.KeyValuePairsRelease(kvpPtr);
        }

        return this;
    }

    /// <summary>
    /// Enable or disable streaming mode. When enabled, a native streaming callback is installed
    /// on the session. Use <see cref="ProcessStreamingRequestAsync"/> to receive items as they are generated.
    /// The callback remains installed until disabled or the session is disposed.
    /// </summary>
    /// <returns>This session (fluent).</returns>
    public Session SetStreaming(bool enabled)
    {
        ThrowIfDisposed();

        if (enabled && _nativeStreamingCallback == null)
        {
            _nativeStreamingCallback = (FlStreamingCallbackData data, IntPtr userData) =>
            {
                var channel = _activeChannel;
                if (channel == null)
                {
                    return 0;
                }

                try
                {
                    if (data.ItemQueue != IntPtr.Zero)
                    {
                        while (Api.Item.QueueTryPop(data.ItemQueue, out var itemPtr))
                        {
                            // Ownership transfers to the channel consumer who disposes it
#pragma warning disable IDISP001
                            var item = Item.FromNative(itemPtr, ownsHandle: true);
#pragma warning restore IDISP001
                            if (!channel.Writer.TryWrite(item))
                            {
                                item.Dispose();
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    channel.Writer.TryComplete(
                        new FoundryLocalException("Error processing streaming callback data.", ex));
                    return 1;
                }

                return _streamingCt.IsCancellationRequested ? 1 : 0;
            };

            _session.SetStreamingCallback(_nativeStreamingCallback);
        }
        else if (!enabled && _nativeStreamingCallback != null)
        {
            _session.SetStreamingCallback(null);
            _nativeStreamingCallback = null;
        }

        return this;
    }

    /// <summary>
    /// Process a request and return the complete response.
    /// </summary>
    public async Task<Response> ProcessRequestAsync(Request request, CancellationToken ct = default)
    {
        ThrowIfDisposed();

        return await Task.Run(() =>
        {
            var responsePtr = _session.ProcessRequest(request.Ptr);
            return new Response(responsePtr);
        }, ct).ConfigureAwait(false);
    }

    /// <summary>
    /// Process a request with streaming. Returns items as they are produced.
    /// Requires <see cref="SetStreaming"/> to have been called with <c>true</c>.
    /// Concurrent streaming requests on the same session are not supported.
    /// </summary>
    /// <exception cref="InvalidOperationException">
    /// Thrown if streaming has not been enabled via <see cref="SetStreaming"/>, or if another
    /// streaming request is already in flight on this session (concurrent streaming requests
    /// on the same session are not supported).
    /// </exception>
    public async IAsyncEnumerable<Item> ProcessStreamingRequestAsync(
        Request request,
        [EnumeratorCancellation] CancellationToken ct = default)
    {
        ThrowIfDisposed();

        if (_nativeStreamingCallback == null)
        {
            throw new InvalidOperationException(
                "Streaming not enabled. Call SetStreaming(true) before ProcessStreamingRequestAsync.");
        }

        var channel = Channel.CreateUnbounded<Item>(
            new UnboundedChannelOptions
            {
                SingleWriter = true,
                SingleReader = true,
                AllowSynchronousContinuations = true,
            });

        if (Interlocked.CompareExchange(ref _activeChannel, channel, null) != null)
        {
            throw new InvalidOperationException(
                "Concurrent streaming requests on the same session are not supported. "
                + "Drain or cancel the in-flight stream before starting another.");
        }

        _streamingCt = ct;

        var task = Task.Run(() =>
        {
            try
            {
                var responsePtr = _session.ProcessRequest(request.Ptr);
                Api.Inference.ResponseRelease(responsePtr);
                channel.Writer.TryComplete();
            }
            catch (OperationCanceledException)
            {
                channel.Writer.TryComplete();
            }
            catch (Exception ex)
            {
                channel.Writer.TryComplete(new FoundryLocalException("Error executing streaming request.", ex));
            }
            finally
            {
                Interlocked.Exchange(ref _activeChannel, null);
            }
        }, ct);

        await foreach (var item in channel.Reader.ReadAllAsync(ct).ConfigureAwait(false))
        {
            yield return item;
        }

        await task.ConfigureAwait(false);
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                _session.Dispose();
            }

            _disposed = true;
        }
    }

    protected NativeSession GetNativeSession() { return _session; }

    protected void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
    }
}
