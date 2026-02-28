// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Collections.Generic;
using System.Threading.Tasks;

using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging.Abstractions;

using Moq;

/// <summary>
/// Unit tests that verify ChatClient streaming propagates native-layer errors reported via
/// Response.Error to the caller instead of silently completing with an empty stream.
/// </summary>
internal sealed class ChatClientStreamingErrorTests
{
    [Test]
    public async Task ChatStreaming_NativeErrorWithNoCallbacks_ThrowsFoundryLocalException()
    {
        // Arrange: native layer returns an error without invoking any streaming callbacks
        var mockInterop = new Mock<ICoreInterop>();
        mockInterop
            .Setup(x => x.ExecuteCommandWithCallbackAsync(
                It.IsAny<string>(),
                It.IsAny<CoreInteropRequest?>(),
                It.IsAny<ICoreInterop.CallbackFn>(),
                It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Error = "Native error: invalid model" });

        var logger = NullLogger<OpenAIChatClient>.Instance;
        var chatClient = new OpenAIChatClient("test-model", mockInterop.Object, logger);

        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "user", Content = "Hello" }
        ];

        // Act
        FoundryLocalException? caught = null;
        try
        {
            await foreach (var _ in chatClient.CompleteChatStreamingAsync(messages, CancellationToken.None))
            {
            }
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }

        // Assert: a FoundryLocalException must have been thrown
        await Assert.That(caught).IsNotNull();
    }

    [Test]
    public async Task ChatStreaming_NativeErrorWithNoCallbacks_ErrorMessagePropagated()
    {
        // Arrange
        const string nativeError = "Native error: invalid model";
        var mockInterop = new Mock<ICoreInterop>();
        mockInterop
            .Setup(x => x.ExecuteCommandWithCallbackAsync(
                It.IsAny<string>(),
                It.IsAny<CoreInteropRequest?>(),
                It.IsAny<ICoreInterop.CallbackFn>(),
                It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Error = nativeError });

        var logger = NullLogger<OpenAIChatClient>.Instance;
        var chatClient = new OpenAIChatClient("test-model", mockInterop.Object, logger);

        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "user", Content = "Hello" }
        ];

        // Act
        FoundryLocalException? caught = null;
        try
        {
            await foreach (var _ in chatClient.CompleteChatStreamingAsync(messages, CancellationToken.None))
            {
            }
        }
        catch (FoundryLocalException ex)
        {
            caught = ex;
        }

        // Assert: the exception message should contain the native error text
        await Assert.That(caught).IsNotNull();
        await Assert.That(caught!.Message).Contains(nativeError);
    }

    [Test]
    public async Task ChatStreaming_NoError_CompletesSuccessfully()
    {
        // Arrange: native layer returns success without invoking any callbacks
        // (empty stream, but no error)
        var mockInterop = new Mock<ICoreInterop>();
        mockInterop
            .Setup(x => x.ExecuteCommandWithCallbackAsync(
                It.IsAny<string>(),
                It.IsAny<CoreInteropRequest?>(),
                It.IsAny<ICoreInterop.CallbackFn>(),
                It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Error = null, Data = null });

        var logger = NullLogger<OpenAIChatClient>.Instance;
        var chatClient = new OpenAIChatClient("test-model", mockInterop.Object, logger);

        List<ChatMessage> messages =
        [
            new ChatMessage { Role = "user", Content = "Hello" }
        ];

        // Act & Assert: no exception, just an empty stream
        var itemCount = 0;
        await foreach (var _ in chatClient.CompleteChatStreamingAsync(messages, CancellationToken.None))
        {
            itemCount++;
        }

        await Assert.That(itemCount).IsEqualTo(0);
    }
}
