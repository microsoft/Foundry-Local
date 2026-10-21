using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging.Abstractions;

var configuration = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = LogLevel.Information,
};

await FoundryLocalManager.CreateAsync(configuration, NullLogger.Instance).ConfigureAwait(false);
using var manager = FoundryLocalManager.Instance;

Console.WriteLine("Discovering execution providers:");
var executionProviders = manager.DiscoverEps();
foreach (var executionProvider in executionProviders)
{
    Console.WriteLine($"  {executionProvider.Name} (registered: {executionProvider.IsRegistered})");
}

Console.WriteLine("Downloading and registering execution providers...");
await manager.DownloadAndRegisterEpsAsync(
    (name, progress) => Console.Write($"\r  {name}: {progress:F1}%   "))
    .ConfigureAwait(false);
Console.WriteLine();

Console.WriteLine("Opening the public catalog...");
var catalog = await manager.GetCatalogAsync().ConfigureAwait(false);
var model = await catalog.GetModelAsync("qwen2.5-0.5b").ConfigureAwait(false)
    ?? throw new InvalidOperationException("The qwen2.5-0.5b model was not found.");

if (!await model.IsCachedAsync().ConfigureAwait(false))
{
    Console.WriteLine($"Downloading {model.Id}...");
    await model.DownloadAsync(
        progress => Console.Write($"\r  {progress:F1}%   "))
        .ConfigureAwait(false);
    Console.WriteLine();
}
else
{
    Console.WriteLine($"{model.Id} is already cached; skipping download.");
}

Console.WriteLine($"Loading {model.Id}...");
await model.LoadAsync().ConfigureAwait(false);

try
{
    using var session = new ChatSession(model);
    using var request = new Request();
    request.AddItem(MessageItem.User("Why is the sky blue? Answer in two sentences."));
    request.SetOptions(new RequestOptions
    {
        Search = new SearchOptions { Temperature = 0.0f, MaxOutputTokens = 128 },
    });

    Console.WriteLine("Running native ChatSession inference...");
    using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

    foreach (var item in response)
    {
        using (item)
        {
            if (item is MessageItem message)
            {
                Console.WriteLine($"Assistant: {message.GetSimpleText()}");
            }
        }
    }
}
finally
{
    Console.WriteLine($"Unloading {model.Id}...");
    await model.UnloadAsync().ConfigureAwait(false);
}
