// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

namespace Microsoft.AI.Foundry.Local.SmokeNetFx;

using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging.Abstractions;

/// <summary>
/// Sanity check for the netstandard2.0 build of the SDK consumed from .NET Framework.
///
/// Exercises the runtime paths that differ between modern .NET and netstandard2.0:
/// <list type="bullet">
/// <item><description>DllLoader.NetStandard.cs (LoadLibraryW + eager preload)</description></item>
/// <item><description>Utf8.PtrToString unsafe-pointer fallback (no Marshal.PtrToStringUTF8)</description></item>
/// <item><description>Utf8.StringToCoTaskMem fallback (no Marshal.StringToCoTaskMemUTF8)</description></item>
/// <item><description>Polyfill packages: System.Memory, System.Threading.Channels, Microsoft.Bcl.AsyncInterfaces</description></item>
/// <item><description>Async/await + IAsyncEnumerable (streaming) on .NET Framework</description></item>
/// </list>
///
/// Resolves the model cache from <c>TEST_MODEL_CACHE_DIR</c> if set, otherwise
/// <c>&lt;repoRoot&gt;/../test-data-shared</c> — the same convention the main test suite uses
/// (see Utils.cs in FoundryLocal.Tests). This lets the smoke run reuse any model already
/// downloaded by the integration tests instead of pulling it again.
///
/// Returns 0 on success, non-zero on any failure.
/// </summary>
internal static class Program
{
    // Smallest cached model used by the regular test suite — keeps this fast.
    private const string ModelAlias = "qwen2.5-0.5b-instruct-generic-cpu:4";

    private static int Main()
    {
        try
        {
            RunAsync().GetAwaiter().GetResult();
            Console.WriteLine("[Smoke.NetFx] OK");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("[Smoke.NetFx] FAIL: " + ex);
            return 1;
        }
    }

    private static async Task RunAsync()
    {
        var cacheDir = ResolveTestModelCacheDir();
        Console.WriteLine("[Smoke.NetFx] model cache dir: " + cacheDir);

        var configuration = new Configuration
        {
            AppName = "Smoke.NetFx",
            ModelCacheDir = cacheDir,
        };
        await FoundryLocalManager.CreateAsync(configuration, NullLogger.Instance).ConfigureAwait(false);

        var manager = FoundryLocalManager.Instance;
        var catalog = await manager.GetCatalogAsync().ConfigureAwait(false);

        var model = await catalog.GetModelVariantAsync(ModelAlias).ConfigureAwait(false)
            ?? throw new InvalidOperationException("Model not found in catalog: " + ModelAlias);

        var cachedBeforeDownload = await model.IsCachedAsync().ConfigureAwait(false);
        Console.WriteLine("[Smoke.NetFx] " + ModelAlias + " cached before download: " + cachedBeforeDownload);

        Console.WriteLine("[Smoke.NetFx] downloading (no-op if already cached)...");
        await model.DownloadAsync(p => Console.WriteLine($"[Smoke.NetFx]   download progress: {p:0.0}%"))
                   .ConfigureAwait(false);

        if (!await model.IsCachedAsync().ConfigureAwait(false))
        {
            throw new InvalidOperationException("Model still not cached after DownloadAsync.");
        }

        Console.WriteLine("[Smoke.NetFx] loading model...");
        await model.LoadAsync().ConfigureAwait(false);

        try
        {
            await NonStreamingAsync(model).ConfigureAwait(false);
            await StreamingAsync(model).ConfigureAwait(false);
        }
        finally
        {
            // Unload + dispose so the native side releases the loaded model and
            // we exit cleanly (no OGA leak warnings on stderr → exit code 0).
            await model.UnloadAsync().ConfigureAwait(false);
            manager.Dispose();
        }
    }

    private static async Task NonStreamingAsync(IModel model)
    {
        using var session = new ChatSession(model);
        using var request = new Request();
        request.AddItem(MessageItem.User("What is 7 multiplied by 6? Reply with the number only."));

        using var response = await session.ProcessRequestAsync(request).ConfigureAwait(false);

        string? text = null;
        foreach (var item in response)
        {
            using (item)
            {
                if (item is MessageItem msg)
                {
                    text = msg.GetSimpleText();
                }
            }
        }

        Console.WriteLine("[Smoke.NetFx] non-streaming response: " + (text ?? "<null>"));
        if (string.IsNullOrEmpty(text))
        {
            throw new InvalidOperationException("Empty non-streaming response.");
        }
    }

    private static async Task StreamingAsync(IModel model)
    {
        using var session = new ChatSession(model);
        session.SetStreaming(true);

        using var request = new Request();
        request.AddItem(MessageItem.User("Count from 1 to 3."));

        var sb = new System.Text.StringBuilder();
        await foreach (var item in session.ProcessStreamingRequestAsync(request).ConfigureAwait(false))
        {
            using (item)
            {
                if (item is TextItem txt)
                {
                    sb.Append(txt.Text);
                }
            }
        }

        var streamed = sb.ToString();
        Console.WriteLine("[Smoke.NetFx] streaming response: " + streamed);
        if (string.IsNullOrEmpty(streamed))
        {
            throw new InvalidOperationException("Empty streaming response.");
        }
    }

    /// <summary>
    /// Mirrors the resolution used by FoundryLocal.Tests Utils.AssemblyInit so this
    /// app shares the cached model directory with the main test suite.
    /// </summary>
    private static string ResolveTestModelCacheDir()
    {
        var env = Environment.GetEnvironmentVariable("TEST_MODEL_CACHE_DIR");
        if (!string.IsNullOrWhiteSpace(env) && Directory.Exists(env))
        {
            return Path.GetFullPath(env);
        }

        // Default convention: a sibling of the repo root named "test-data-shared".
        var path = Path.GetFullPath(Path.Combine(GetRepoRoot(), "..", "test-data-shared"));
        Directory.CreateDirectory(path);
        return path;
    }

    // Walks up from this source file's directory looking for the .git folder.
    private static string GetRepoRoot([CallerFilePath] string sourceFile = "")
    {
        var dir = new DirectoryInfo(Path.GetDirectoryName(sourceFile) ?? Environment.CurrentDirectory);
        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, ".git")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        throw new InvalidOperationException("Could not find git repository root from " + sourceFile);
    }
}
