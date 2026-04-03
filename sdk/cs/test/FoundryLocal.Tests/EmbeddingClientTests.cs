// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Threading.Tasks;

internal sealed class EmbeddingClientTests
{
    private static IModel? model;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance; // initialized by Utils
        var catalog = await manager.GetCatalogAsync();

        // Load the specific cached model variant directly
        var model = await catalog.GetModelVariantAsync("qwen3-0.6b-embedding-generic-cpu:1").ConfigureAwait(false);
        await Assert.That(model).IsNotNull();

        await model!.LoadAsync().ConfigureAwait(false);
        await Assert.That(await model.IsLoadedAsync()).IsTrue();

        EmbeddingClientTests.model = model;
    }

    [Test]
    public async Task Embedding_BasicRequest_Succeeds()
    {
        var embeddingClient = await model!.GetEmbeddingClientAsync();
        await Assert.That(embeddingClient).IsNotNull();

        var response = await embeddingClient.GenerateEmbeddingAsync("The quick brown fox jumps over the lazy dog")
                                             .ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Model).IsEqualTo("qwen3-0.6b-embedding-generic-cpu:1");
        await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response.Data[0].Embedding).IsNotNull();
        await Assert.That(response.Data[0].Embedding.Count).IsEqualTo(1024);
        await Assert.That(response.Data[0].Index).IsEqualTo(0);

        Console.WriteLine($"Embedding dimension: {response.Data[0].Embedding.Count}");
        Console.WriteLine($"First value: {response.Data[0].Embedding[0]}");
        Console.WriteLine($"Last value: {response.Data[0].Embedding[1023]}");
    }

    [Test]
    public async Task Embedding_IsNormalized()
    {
        var embeddingClient = await model!.GetEmbeddingClientAsync();
        await Assert.That(embeddingClient).IsNotNull();

        var inputs = new[]
        {
            "The quick brown fox jumps over the lazy dog",
            "Machine learning is a subset of artificial intelligence",
            "The capital of France is Paris"
        };

        foreach (var input in inputs)
        {
            var response = await embeddingClient.GenerateEmbeddingAsync(input).ConfigureAwait(false);

            await Assert.That(response).IsNotNull();
            await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();

            var embedding = response.Data[0].Embedding;

            await Assert.That(embedding.Count).IsEqualTo(1024);

            // Verify L2 norm is approximately 1.0
            double norm = 0;
            foreach (var val in embedding)
            {
                norm += val * val;
            }

            norm = Math.Sqrt(norm);
            await Assert.That(norm).IsGreaterThanOrEqualTo(0.99);
            await Assert.That(norm).IsLessThanOrEqualTo(1.01);

            // All values should be within [-1, 1] for a normalized vector
            foreach (var val in embedding)
            {
                await Assert.That(val).IsGreaterThanOrEqualTo(-1.0);
                await Assert.That(val).IsLessThanOrEqualTo(1.0);
            }
        }
    }

    [Test]
    public async Task Embedding_DifferentInputs_ProduceDifferentEmbeddings()
    {
        var embeddingClient = await model!.GetEmbeddingClientAsync();
        await Assert.That(embeddingClient).IsNotNull();

        var response1 = await embeddingClient.GenerateEmbeddingAsync("The quick brown fox").ConfigureAwait(false);
        var response2 = await embeddingClient.GenerateEmbeddingAsync("The capital of France is Paris").ConfigureAwait(false);

        await Assert.That(response1).IsNotNull();
        await Assert.That(response2).IsNotNull();
        await Assert.That(response1.Data).IsNotNull().And.IsNotEmpty();
        await Assert.That(response2.Data).IsNotNull().And.IsNotEmpty();

        // Same dimensionality
        await Assert.That(response1.Data[0].Embedding.Count)
            .IsEqualTo(response2.Data[0].Embedding.Count);

        // But different values (cosine similarity should not be 1.0)
        double dot = 0;
        for (int i = 0; i < response1.Data[0].Embedding.Count; i++)
        {
            dot += response1.Data[0].Embedding[i] * response2.Data[0].Embedding[i];
        }

        await Assert.That(dot).IsLessThan(0.99);
    }

    [Test]
    public async Task Embedding_SameInput_ProducesSameEmbedding()
    {
        var embeddingClient = await model!.GetEmbeddingClientAsync();
        await Assert.That(embeddingClient).IsNotNull();

        var input = "Deterministic embedding test";

        var response1 = await embeddingClient.GenerateEmbeddingAsync(input).ConfigureAwait(false);
        var response2 = await embeddingClient.GenerateEmbeddingAsync(input).ConfigureAwait(false);

        await Assert.That(response1).IsNotNull();
        await Assert.That(response2).IsNotNull();
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
    public async Task Embedding_KnownValues_CapitalOfFrance()
    {
        var embeddingClient = await model!.GetEmbeddingClientAsync();
        await Assert.That(embeddingClient).IsNotNull();

        var response = await embeddingClient.GenerateEmbeddingAsync("The capital of France is Paris")
                                             .ConfigureAwait(false);
        await Assert.That(response).IsNotNull();
        await Assert.That(response.Data).IsNotNull().And.IsNotEmpty();
        var embedding = response.Data[0].Embedding;

        await Assert.That(embedding.Count).IsEqualTo(1024);

        // Use tolerance for float32 model outputs which may vary across platforms
        const double tolerance = 1e-5;
        await Assert.That(Math.Abs(embedding[0] - (-0.023386012762784958))).IsLessThanOrEqualTo(tolerance);
        await Assert.That(Math.Abs(embedding[1023] - (-0.011731955222785473))).IsLessThanOrEqualTo(tolerance);
    }

}
