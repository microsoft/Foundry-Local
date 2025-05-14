using FoundryLocal;

var manager = new FoundryManager();
await manager.start_service();
foreach (var m in await manager.list_catalog_models())
{
    Console.WriteLine($"Model: {m.Alias} ({m.Id})");
    Console.WriteLine($"  Version: {m.Version}");
    Console.WriteLine($"  Runtime: {m.Runtime}");
    Console.WriteLine($"  Uri: {m.Uri}");
    Console.WriteLine($"  ModelSizeInMb: {m.FileSizeMb}");
    Console.WriteLine($"  PromptTemplate: {m.PromptTemplate}");
    Console.WriteLine($"  Provider: {m.Provider}");
}
Console.WriteLine($"Model cache location at {await manager.get_cache_location()}");