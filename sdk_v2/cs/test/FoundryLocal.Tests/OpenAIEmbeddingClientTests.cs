// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;

using TUnit.Core.Exceptions;

internal sealed class OpenAIEmbeddingClientTests
{
    private static IModel? model;
    private static OpenAIEmbeddingClient? client;

    [Before(Class)]
    public static async Task Setup()
    {
        try
        {
            var manager = FoundryLocalManager.Instance;
            var catalog = await manager.GetCatalogAsync();

            // Try the well-known embedding model first.
            var embeddingModel = await catalog.GetModelAsync("qwen3-0.6b-embedding-generic-cpu:1").ConfigureAwait(false);

            if (embeddingModel == null)
            {
                // Fallback: scan the catalog for any embeddings model.
                var allModels = await catalog.ListModelsAsync().ConfigureAwait(false);
                embeddingModel = allModels.FirstOrDefault(m => m.Info.Task == "embeddings");
            }

            if (embeddingModel == null)
            {
                return; // No embedding model in catalog — tests will skip individually.
            }

            if (!await embeddingModel.IsCachedAsync())
            {
                return;
            }

            await embeddingModel.LoadAsync().ConfigureAwait(false);
            await Assert.That(await embeddingModel.IsLoadedAsync()).IsTrue();

            OpenAIEmbeddingClientTests.model = embeddingModel;
            OpenAIEmbeddingClientTests.client = await embeddingModel.GetEmbeddingClientAsync().ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Setup failed: {ex}");
            throw;
        }
    }

    [After(Class)]
    public static Task Cleanup()
    {
        // Do NOT call model.UnloadAsync() here. TUnit runs test classes in parallel,
        // so this hook can fire while a sibling class is still using the same model
        // (which now causes UnloadModel to throw). The assembly-level cleanup hook
        // performs a single best-effort unload pass after all classes have finished.
        return Task.CompletedTask;
    }

    [Test]
    public async Task GenerateEmbedding_BasicRequest_Succeeds()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var response = await client.GenerateEmbeddingAsync("The quick brown fox jumps over the lazy dog")
                                   .ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Model).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Data[0].Embedding).IsNotNull();
        await Assert.That(response.Data[0].Embedding.Count).IsGreaterThan(0);
        await Assert.That(response.Data[0].Index).IsEqualTo(0);

        Console.WriteLine($"Embedding dimension: {response.Data[0].Embedding.Count}, model: {response.Model}");
    }

    [Test]
    public async Task GenerateEmbedding_IsL2Normalized()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var inputs = new[]
        {
            "The quick brown fox jumps over the lazy dog",
            "Machine learning is a subset of artificial intelligence",
            "The capital of France is Paris",
        };

        foreach (var input in inputs)
        {
            var response = await client.GenerateEmbeddingAsync(input).ConfigureAwait(false);

            await Assert.That(response).IsNotNull();
            await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();

            var embedding = response.Data[0].Embedding;
            await Assert.That(embedding.Count).IsGreaterThan(0);

            double norm = 0;
            foreach (var val in embedding)
            {
                norm += val * val;
            }

            norm = Math.Sqrt(norm);
            await Assert.That(norm).IsGreaterThanOrEqualTo(0.99);
            await Assert.That(norm).IsLessThanOrEqualTo(1.01);

            foreach (var val in embedding)
            {
                await Assert.That(val).IsGreaterThanOrEqualTo(-1.0);
                await Assert.That(val).IsLessThanOrEqualTo(1.0);
            }
        }
    }

    [Test]
    public async Task GenerateEmbedding_DistinctInputs_ProduceDistinctEmbeddings()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var response1 = await client.GenerateEmbeddingAsync("The quick brown fox").ConfigureAwait(false);
        var response2 = await client.GenerateEmbeddingAsync("The capital of France is Paris").ConfigureAwait(false);

        await Assert.That(response1.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response2.Data).IsNotNull().And.IsNotEmpty();

        await Assert.That(response1.Data[0].Embedding.Count)
            .IsEqualTo(response2.Data[0].Embedding.Count);

        double dot = 0;
        for (int i = 0; i < response1.Data[0].Embedding.Count; i++)
        {
            dot += response1.Data[0].Embedding[i] * response2.Data[0].Embedding[i];
        }

        await Assert.That(dot).IsLessThan(0.99);
    }

    [Test]
    public async Task GenerateEmbedding_SameInput_IsDeterministic()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var input = "Deterministic embedding test";

        var response1 = await client.GenerateEmbeddingAsync(input).ConfigureAwait(false);
        var response2 = await client.GenerateEmbeddingAsync(input).ConfigureAwait(false);

        await Assert.That(response1.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response2.Data).IsNotNull().And.IsNotEmpty();

        await Assert.That(response1.Data[0].Embedding.Count)
            .IsEqualTo(response2.Data[0].Embedding.Count);

        for (int i = 0; i < response1.Data[0].Embedding.Count; i++)
        {
            await Assert.That(response1.Data[0].Embedding[i])
                .IsEqualTo(response2.Data[0].Embedding[i]);
        }
    }

    [Test]
    public async Task GenerateEmbedding_EmptyString_ThrowsArgumentException()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        await Assert.That(async () => await client.GenerateEmbeddingAsync("").ConfigureAwait(false))
            .Throws<ArgumentException>();
    }

    [Test]
    public async Task GenerateEmbeddings_EmptyBatch_ThrowsArgumentException()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        await Assert.That(async () => await client.GenerateEmbeddingsAsync(Array.Empty<string>()).ConfigureAwait(false))
            .Throws<ArgumentException>();
    }

    [Test]
    public async Task GenerateEmbeddings_Batch_ReturnsOneEmbeddingPerInput()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var response = await client.GenerateEmbeddingsAsync(new[]
        {
            "The quick brown fox jumps over the lazy dog",
            "Machine learning is a subset of artificial intelligence",
            "The capital of France is Paris",
        }).ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Data.Count).IsEqualTo(3);

        var dim = response.Data[0].Embedding.Count;
        await Assert.That(dim).IsGreaterThan(0);

        for (var i = 0; i < 3; i++)
        {
            await Assert.That(response.Data[i].Index).IsEqualTo(i);
            await Assert.That(response.Data[i].Embedding.Count).IsEqualTo(dim);
        }
    }

    [Test]
    public async Task GenerateEmbeddings_BatchVsSingle_AreEquivalent()
    {
        if (client == null)
        {
            throw new SkipTestException("Embedding client not available");
        }

        var input = "The capital of France is Paris";

        var singleResponse = await client.GenerateEmbeddingAsync(input).ConfigureAwait(false);
        var batchResponse = await client.GenerateEmbeddingsAsync(new[] { input }).ConfigureAwait(false);

        await Assert.That(singleResponse.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(batchResponse.Data.Count).IsEqualTo(1);
        await Assert.That(batchResponse.Data[0].Embedding.Count)
            .IsEqualTo(singleResponse.Data[0].Embedding.Count);

        for (var i = 0; i < singleResponse.Data[0].Embedding.Count; i++)
        {
            await Assert.That(batchResponse.Data[0].Embedding[i])
                .IsEqualTo(singleResponse.Data[0].Embedding[i]);
        }
    }
}
