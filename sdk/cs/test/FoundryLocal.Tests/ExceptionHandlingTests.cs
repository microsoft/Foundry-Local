// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Threading.Tasks;

using Microsoft.Extensions.Logging;

using Moq;

using SdkUtils = Microsoft.AI.Foundry.Local.Utils;

internal sealed class ExceptionHandlingTests
{
    private readonly Mock<ILogger> _mockLogger = new();

    [Test]
    public async Task CallWithExceptionHandling_SuccessfulFunc_ReturnsResult()
    {
        var result = SdkUtils.CallWithExceptionHandling(() => 42, "error msg", _mockLogger.Object);

        await Assert.That(result).IsEqualTo(42);
    }

    [Test]
    public async Task CallWithExceptionHandling_GenericException_WrapsInFoundryLocalException()
    {
        var original = new InvalidOperationException("boom");

        await Assert.That(() =>
            SdkUtils.CallWithExceptionHandling<int>(
                () => throw original,
                "wrapper message",
                _mockLogger.Object))
            .Throws<FoundryLocalException>()
            .WithMessage("wrapper message");
    }

    [Test]
    public async Task CallWithExceptionHandling_FoundryLocalException_RethrowsDirectly()
    {
        var original = new FoundryLocalException("direct error");

        try
        {
            SdkUtils.CallWithExceptionHandling<int>(
                () => throw original,
                "should not wrap",
                _mockLogger.Object);
        }
        catch (FoundryLocalException ex)
        {
            // Should be the SAME exception, not a new wrapper
            await Assert.That(ex).IsSameReferenceAs(original);
            return;
        }

        throw new Exception("Expected FoundryLocalException was not thrown");
    }

    [Test]
    public async Task CallWithExceptionHandling_OperationCanceledException_PropagatesUnchanged()
    {
        var original = new OperationCanceledException("cancelled");

        try
        {
            SdkUtils.CallWithExceptionHandling<int>(
                () => throw original,
                "should not wrap",
                _mockLogger.Object);
        }
        catch (OperationCanceledException ex)
        {
            await Assert.That(ex).IsSameReferenceAs(original);
            return;
        }

        throw new Exception("Expected OperationCanceledException was not thrown");
    }

    [Test]
    public async Task CallWithExceptionHandling_TaskCanceledException_PropagatesUnchanged()
    {
        var original = new TaskCanceledException("task cancelled");

        try
        {
            SdkUtils.CallWithExceptionHandling<int>(
                () => throw original,
                "should not wrap",
                _mockLogger.Object);
        }
        catch (TaskCanceledException ex)
        {
            await Assert.That(ex).IsSameReferenceAs(original);
            return;
        }

        throw new Exception("Expected TaskCanceledException was not thrown");
    }

    [Test]
    public Task CallWithExceptionHandling_GenericException_LogsViaLogger()
    {
        try
        {
            SdkUtils.CallWithExceptionHandling<int>(
                () => throw new InvalidOperationException("boom"),
                "error context",
                _mockLogger.Object);
        }
        catch (FoundryLocalException)
        {
            // Expected — now verify the logger was called
        }

        _mockLogger.Verify(
            l => l.Log(
                LogLevel.Error,
                It.IsAny<EventId>(),
                It.Is<It.IsAnyType>((v, _) => v.ToString()!.Contains("error context")),
                It.IsAny<Exception>(),
                It.IsAny<Func<It.IsAnyType, Exception?, string>>()),
            Times.Once);

        return Task.CompletedTask;
    }
}
