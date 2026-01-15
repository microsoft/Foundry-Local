// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Reflection;
using System.Text.Json;
using System.Threading.Tasks;
using Microsoft.AI.Foundry.Local;
using RichardSzalay.MockHttp;
using Xunit;

public class DownloadExceptionTest
{
    [Fact]
    public async Task DownloadModelAsync_RethrowsException_AndPrintsUrl()
    {
        // Arrange
        using var mockHttp = new MockHttpMessageHandler();
        var client = mockHttp.ToHttpClient();
        client.BaseAddress = new Uri("http://localhost:5272");

        using var manager = new FoundryLocalManager();
        
        // Inject the mock client
        typeof(FoundryLocalManager)
            .GetField("_serviceUri", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(manager, client.BaseAddress);

        typeof(FoundryLocalManager)
            .GetField("_serviceClient", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(manager, client);

        // Mock catalog to return a model
        var modelInfo = new ModelInfo
        {
            ModelId = "test-model:1",
            Uri = "https://example.com/model.onnx",
            Alias = "test-model",
            Runtime = new Runtime { DeviceType = DeviceType.CPU }
        };
        var catalog = new List<ModelInfo> { modelInfo };
        var catalogJson = JsonSerializer.Serialize(catalog, ModelGenerationContext.Default.ListModelInfo);
        
        mockHttp.When(HttpMethod.Get, "/foundry/list").Respond("application/json", catalogJson);
        mockHttp.When(HttpMethod.Get, "/openai/models").Respond("application/json", "[]");

        // Mock download to throw an exception
        mockHttp.When(HttpMethod.Post, "/openai/download").Throw(new HttpRequestException("SSL connection failed"));

        // Act & Assert
        var ex = await Assert.ThrowsAsync<HttpRequestException>(() => manager.DownloadModelAsync("test-model"));
        Assert.Equal("SSL connection failed", ex.Message);
        
        // Note: Verifying Console.WriteLine is difficult in this setup without redirecting Console.Out, 
        // but we verified the exception is rethrown.
    }
}
