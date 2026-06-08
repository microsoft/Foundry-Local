// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
// </imports>

// <init>
var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize the singleton instance.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;
// </init>

// <model_setup>
// Get the model catalog
var catalog = await mgr.GetCatalogAsync();

// Get an embedding model
var model = await catalog.GetModelAsync("qwen3-embedding-0.6b") ?? throw new Exception("Embedding model not found");

// Download the model (the method skips download if already cached)
await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f)
    {
        Console.WriteLine();
    }
});

// Load the model
Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");
// </model_setup>

// <single_embedding>
// Get an embedding client
var embeddingClient = await model.GetEmbeddingClientAsync();

// Generate a single embedding
Console.WriteLine("\n--- Single Embedding ---");
var response = await embeddingClient.GenerateEmbeddingAsync("The quick brown fox jumps over the lazy dog");
var embedding = response.Data[0].Embedding;
Console.WriteLine($"Dimensions: {embedding.Count}");
Console.WriteLine($"First 5 values: [{string.Join(", ", embedding.Take(5).Select(v => v.ToString("F6")))}]");
// </single_embedding>

// <batch_embedding>
// Generate embeddings for multiple inputs
Console.WriteLine("\n--- Batch Embeddings ---");
var batchResponse = await embeddingClient.GenerateEmbeddingsAsync([
    "Machine learning is a subset of artificial intelligence",
    "The capital of France is Paris",
    "Rust is a systems programming language"
]);

Console.WriteLine($"Number of embeddings: {batchResponse.Data.Count}");
for (var i = 0; i < batchResponse.Data.Count; i++)
{
    Console.WriteLine($"  [{i}] Dimensions: {batchResponse.Data[i].Embedding.Count}");
}
// </batch_embedding>

// <cleanup>
// Tidy up - unload the model
await model.UnloadAsync();
Console.WriteLine("\nModel unloaded.");
// </cleanup>
// </complete_code>
