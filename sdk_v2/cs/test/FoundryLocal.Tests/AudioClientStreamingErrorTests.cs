// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Logging.Abstractions;

using Moq;

/// <summary>
/// Unit tests that verify AudioClient streaming propagates native-layer errors reported via
/// Response.Error to the caller instead of silently completing with an empty stream.
/// </summary>
internal sealed class AudioClientStreamingErrorTests
{
    [Test]
    public async Task AudioStreaming_NativeErrorWithNoCallbacks_ThrowsFoundryLocalException()
    {
        // Arrange: native layer returns an error without invoking any streaming callbacks
        var mockInterop = new Mock<ICoreInterop>();
        mockInterop
            .Setup(x => x.ExecuteCommandWithCallbackAsync(
                It.IsAny<string>(),
                It.IsAny<CoreInteropRequest?>(),
                It.IsAny<ICoreInterop.CallbackFn>(),
                It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Error = "Native error: missing audio file" });

        var logger = NullLogger<OpenAIAudioClient>.Instance;
        var audioClient = new OpenAIAudioClient("test-model", mockInterop.Object, logger);

        // Act
        FoundryLocalException? caught = null;
        try
        {
            await foreach (var _ in audioClient.TranscribeAudioStreamingAsync("nonexistent.mp3", CancellationToken.None))
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
    public async Task AudioStreaming_NativeErrorWithNoCallbacks_ErrorMessagePropagated()
    {
        // Arrange
        const string nativeError = "Native error: missing audio file";
        var mockInterop = new Mock<ICoreInterop>();
        mockInterop
            .Setup(x => x.ExecuteCommandWithCallbackAsync(
                It.IsAny<string>(),
                It.IsAny<CoreInteropRequest?>(),
                It.IsAny<ICoreInterop.CallbackFn>(),
                It.IsAny<CancellationToken?>()))
            .ReturnsAsync(new ICoreInterop.Response { Error = nativeError });

        var logger = NullLogger<OpenAIAudioClient>.Instance;
        var audioClient = new OpenAIAudioClient("test-model", mockInterop.Object, logger);

        // Act
        FoundryLocalException? caught = null;
        try
        {
            await foreach (var _ in audioClient.TranscribeAudioStreamingAsync("nonexistent.mp3", CancellationToken.None))
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
    public async Task AudioStreaming_NoError_CompletesSuccessfully()
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

        var logger = NullLogger<OpenAIAudioClient>.Instance;
        var audioClient = new OpenAIAudioClient("test-model", mockInterop.Object, logger);

        // Act & Assert: no exception, just an empty stream
        var itemCount = 0;
        await foreach (var _ in audioClient.TranscribeAudioStreamingAsync("nonexistent.mp3", CancellationToken.None))
        {
            itemCount++;
        }

        await Assert.That(itemCount).IsEqualTo(0);
    }
}
