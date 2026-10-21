using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging.Abstractions;

var configuration = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = LogLevel.Information,
};

await FoundryLocalManager.CreateAsync(configuration, NullLogger.Instance).ConfigureAwait(false);
using var manager = FoundryLocalManager.Instance;

var catalog = await manager.GetCatalogAsync().ConfigureAwait(false);
var model = await catalog.GetModelAsync("qwen3-embedding-0.6b").ConfigureAwait(false)
    ?? throw new InvalidOperationException("The qwen3-embedding-0.6b model was not found.");

if (!await model.IsCachedAsync().ConfigureAwait(false))
{
    Console.WriteLine($"Downloading {model.Id}...");
    await model.DownloadAsync(
        progress => Console.Write($"\r  {progress:F1}%   "))
        .ConfigureAwait(false);
    Console.WriteLine();
}

await model.LoadAsync().ConfigureAwait(false);

try
{
    using var session = new EmbeddingsSession(model);
    using var request = new Request();
    request.AddItem(new TextItem("The quick brown fox jumps over the lazy dog."));
    request.AddItem(new TextItem("Foundry Local runs models on your device."));

    using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

    for (var index = 0; index < response.ItemCount; index++)
    {
        using var item = response.GetItem(index);
        if (item is not TensorItem tensor)
        {
            throw new InvalidOperationException($"Expected a tensor at index {index}.");
        }

        var values = tensor.ToArray<float>();
        Console.WriteLine(
            $"Embedding {index}: {values.Length} dimensions; first values: "
            + string.Join(", ", values.Take(5).Select(value => value.ToString("F6"))));
    }
}
finally
{
    await model.UnloadAsync().ConfigureAwait(false);
}
