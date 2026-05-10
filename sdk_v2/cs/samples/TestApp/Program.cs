// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------
//
// Foundry Local SDK v2 sample. Demonstrates the typed Session / Request / Response / Item layer.
// Requires foundry_local.dll (+ ORT runtime) at runtime; build alone is enough to validate
// the sample source against the SDK.

using System.Globalization;

using Microsoft.AI.Foundry.Local;

using Microsoft.Extensions.Logging;

// Disambiguate against Microsoft.AI.Foundry.Local.LogLevel.
using LogLevel = Microsoft.Extensions.Logging.LogLevel;

const string ChatAlias = "qwen2.5-0.5b";

// ---------- 1. Initialize ----------------------------------------------------------------------
Console.WriteLine("== 1. Initialize FoundryLocalManager ==");

var config = new Configuration { AppName = "FoundrySampleApp" };

using var loggerFactory = LoggerFactory.Create(b => b
    .AddConsole()
    .SetMinimumLevel(LogLevel.Information));
var logger = loggerFactory.CreateLogger("FoundrySampleApp");

await FoundryLocalManager.CreateAsync(config, logger).ConfigureAwait(false);
var manager = FoundryLocalManager.Instance;

try
{
    // ---------- 2. List the catalog ------------------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 2. List catalog ==");

    var catalog = await manager.GetCatalogAsync().ConfigureAwait(false);
    var models = await catalog.ListModelsAsync().ConfigureAwait(false);
    Console.WriteLine($"  catalog returned {models.Count} model(s); showing first 10:");

    foreach (var m in models.Take(10))
    {
        Console.WriteLine($"    {m.Alias} ({m.Id}) [{m.Info.ModelType}, {m.Info.Task ?? "?"}]");
    }

    // ---------- 3. Pick a chat model -----------------------------------------------------------
    Console.WriteLine();
    Console.WriteLine($"== 3. Pick chat model '{ChatAlias}' ==");

    var chatModel = await catalog.GetModelAsync(ChatAlias).ConfigureAwait(false);
    if (chatModel is null)
    {
        Console.WriteLine($"  '{ChatAlias}' is not in the catalog on this machine. Exiting cleanly.");
        return;
    }

    Console.WriteLine($"  found {chatModel.Alias} ({chatModel.Id})");

    // ---------- 4. Download with progress ------------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 4. Download (cached returns immediately) ==");

    await chatModel.DownloadAsync(progress => Console.Write($"\r  download: {progress:F1}%"))
                   .ConfigureAwait(false);
    Console.WriteLine();

    // ---------- 5. Load -----------------------------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 5. Load ==");
    await chatModel.LoadAsync().ConfigureAwait(false);
    Console.WriteLine("  Loaded.");

    // ---------- 6. ChatSession: non-streaming -------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 6. ChatSession (non-streaming) ==");

    using (var session = new ChatSession(chatModel))
    {
        session.SetOptions(new Dictionary<string, string>
        {
            [SessionParam.MaxOutputTokens] = "200",
            [SessionParam.Temperature] = "0.0",
        });

        using var request = new Request();
        request.AddItem(MessageItem.User("Why is the sky blue? Answer in one sentence."));

        using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

        Console.Write("  assistant: ");
        foreach (var item in response)
        {
            using (item)
            {
                if (item is MessageItem msg && msg.IsSimpleText())
                {
                    Console.Write(msg.GetSimpleText());
                }
                else if (item is TextItem text)
                {
                    Console.Write(text.Text);
                }
            }
        }
        Console.WriteLine();

        var usage = response.GetUsage();
        Console.WriteLine($"  tokens: prompt={usage.PromptTokens}, completion={usage.CompletionTokens}, total={usage.TotalTokens}");
        Console.WriteLine($"  finish: {response.FinishReason}");
    }

    // ---------- 7. ChatSession: streaming -----------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 7. ChatSession (streaming) ==");

    using (var session = new ChatSession(chatModel))
    {
        session.SetStreaming(true);

        using var request = new Request();
        request.AddItem(MessageItem.User("Tell me a haiku about a cat."));

        Console.Write("  haiku: ");
        await foreach (var item in session.ProcessStreamingRequestAsync(request))
        {
            using (item)
            {
                if (item is TextItem text)
                {
                    Console.Write(text.Text);
                }
            }
        }
        Console.WriteLine();
    }

    // ---------- 8. EmbeddingsSession (optional, env-var gated) --------------------------------
    Console.WriteLine();
    Console.WriteLine("== 8. EmbeddingsSession (optional) ==");

    var embedAlias = Environment.GetEnvironmentVariable("FOUNDRY_EMBED_MODEL");
    if (string.IsNullOrWhiteSpace(embedAlias))
    {
        Console.WriteLine("  set FOUNDRY_EMBED_MODEL=<alias> to demo embeddings. Skipping.");
    }
    else
    {
        var embedModel = await catalog.GetModelAsync(embedAlias).ConfigureAwait(false);
        if (embedModel is null)
        {
            Console.WriteLine($"  '{embedAlias}' is not in the catalog. Skipping.");
        }
        else
        {
            await embedModel.DownloadAsync(p => Console.Write($"\r  download: {p:F1}%"))
                            .ConfigureAwait(false);
            Console.WriteLine();
            await embedModel.LoadAsync().ConfigureAwait(false);

            using var session = new EmbeddingsSession(embedModel);

            using var request = new Request();
            // Item.Dispose is a no-op once Request.AddItem has taken ownership;
            // the `using` declarations satisfy the CA2000 analyzer without breaking ownership semantics.
            using var t1 = new TextItem("hello world");
            using var t2 = new TextItem("the quick brown fox");
            request.AddItem(t1);
            request.AddItem(t2);

            using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

            Console.WriteLine($"  produced {response.ItemCount} embedding(s)");
            for (var i = 0; i < response.ItemCount; i++)
            {
                using var item = response.GetItem(i);
                if (item is not TensorItem tensor)
                {
                    Console.WriteLine($"    [{i}] unexpected item type: {item.ItemType}");
                    continue;
                }

                long elements = 1;
                foreach (var dim in tensor.Shape)
                {
                    elements *= dim;
                }

                var preview = new float[Math.Min(5, elements)];
                System.Runtime.InteropServices.Marshal.Copy(tensor.Data, preview, 0, preview.Length);
                var firstFive = string.Join(", ", preview.Select(f => f.ToString("F4", CultureInfo.InvariantCulture)));
                Console.WriteLine($"    [{i}] dims={elements}; first 5: [{firstFive}]");
            }
        }
    }
}
finally
{
    // ---------- 9. Cleanup --------------------------------------------------------------------
    Console.WriteLine();
    Console.WriteLine("== 9. Cleanup ==");
    manager.Dispose();
    Console.WriteLine("  Done.");
}
