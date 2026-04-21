// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using Microsoft.AI.Foundry.Local.Detail;

using Microsoft.Extensions.Logging;

using Moq;

internal sealed class DownloadCancellationTests
{
    [Test]
    public async Task ModelVariantDownload_WithCancellableToken_UsesCallbackPathAndPropagatesCancellation()
    {
        var modelInfo = new ModelInfo
        {
            Id = "test-model-cpu:1",
            Name = "test-model-cpu",
            Alias = "test-model",
            Version = 1,
            ProviderType = "AzureFoundry",
            Uri = "azureml://registries/azureml/models/test-model-cpu/versions/1",
            ModelType = "ONNX",
        };

        var modelLoadManager = new Mock<IModelLoadManager>(MockBehavior.Strict);
        var coreInterop = new Mock<ICoreInterop>(MockBehavior.Strict);
        var logger = new Mock<ILogger>();
        using var cts = new CancellationTokenSource();

        coreInterop.Setup(x => x.ExecuteCommandWithCallbackAsync(
                        It.Is<string>(s => s == "download_model"),
                        It.Is<CoreInteropRequest?>(r => r != null &&
                                                       r.Params != null &&
                                                       r.Params.ContainsKey("Model") &&
                                                       r.Params["Model"] == modelInfo.Id),
                        It.IsAny<ICoreInterop.CallbackFn>(),
                        It.IsAny<CancellationToken?>()))
                   .Returns((string commandName,
                             CoreInteropRequest? request,
                             ICoreInterop.CallbackFn callback,
                             CancellationToken? cancellationToken) =>
                   {
                       callback("10");
                       cts.Cancel();
                       callback("20");
                       return Task.FromResult(new ICoreInterop.Response());
                   });

        var model = new ModelVariant(modelInfo, modelLoadManager.Object, coreInterop.Object, logger.Object);

        OperationCanceledException? caught = null;
        try
        {
            await model.DownloadAsync(ct: cts.Token);
        }
        catch (OperationCanceledException ex)
        {
            caught = ex;
        }

        await Assert.That(caught).IsNotNull();
        coreInterop.Verify(x => x.ExecuteCommandWithCallbackAsync(
                               "download_model",
                               It.IsAny<CoreInteropRequest?>(),
                               It.IsAny<ICoreInterop.CallbackFn>(),
                               It.IsAny<CancellationToken?>()),
                           Times.Once);
        coreInterop.Verify(x => x.ExecuteCommandAsync(
                               It.IsAny<string>(),
                               It.IsAny<CoreInteropRequest?>(),
                               It.IsAny<CancellationToken?>()),
                           Times.Never);
    }
}
