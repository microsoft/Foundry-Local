// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using Microsoft.Extensions.Logging;

using Moq;

internal sealed class FoundryLocalExceptionTests
{
    [Test]
    public async Task PublicCtor_MessageOnly_SetsMessage()
    {
        var ex = new FoundryLocalException("test error");

        await Assert.That(ex.Message).IsEqualTo("test error");
        await Assert.That(ex.InnerException).IsNull();
    }

    [Test]
    public async Task PublicCtor_WithInnerException_PropagatesBoth()
    {
        var inner = new InvalidOperationException("inner");
        var ex = new FoundryLocalException("outer", inner);

        await Assert.That(ex.Message).IsEqualTo("outer");
        await Assert.That(ex.InnerException).IsSameReferenceAs(inner);
    }

    [Test]
    public async Task InternalCtor_WithLogger_LogsError()
    {
        var mockLogger = new Mock<ILogger>();
        var ex = new FoundryLocalException("logged error", mockLogger.Object);

        await Assert.That(ex.Message).IsEqualTo("logged error");

        mockLogger.Verify(
            l => l.Log(
                LogLevel.Error,
                It.IsAny<EventId>(),
                It.Is<It.IsAnyType>((v, _) => v.ToString()!.Contains("logged error")),
                null,
                It.IsAny<Func<It.IsAnyType, Exception?, string>>()),
            Times.Once);
    }

    [Test]
    public async Task InternalCtor_WithInnerExceptionAndLogger_LogsWithInner()
    {
        var mockLogger = new Mock<ILogger>();
        var inner = new InvalidOperationException("inner cause");
        var ex = new FoundryLocalException("logged outer", inner, mockLogger.Object);

        await Assert.That(ex.Message).IsEqualTo("logged outer");
        await Assert.That(ex.InnerException).IsSameReferenceAs(inner);

        mockLogger.Verify(
            l => l.Log(
                LogLevel.Error,
                It.IsAny<EventId>(),
                It.Is<It.IsAnyType>((v, _) => v.ToString()!.Contains("logged outer")),
                inner,
                It.IsAny<Func<It.IsAnyType, Exception?, string>>()),
            Times.Once);
    }
}
