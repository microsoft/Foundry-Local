using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using FoundryPlayground;

// ── Initialize ───────────────────────────────────────────────────────────

var config = new Configuration
{
    AppName = "foundry-local-playground",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
};

await FoundryLocalManager.CreateAsync(config, NullLogger.Instance);
var manager = FoundryLocalManager.Instance;

// ── Discover & download execution providers ──────────────────────────────

Ui.Section("Execution Providers");

var eps = manager.DiscoverEps();
var epList = eps.Select(ep => (ep.Name, ep.IsRegistered)).ToList();
var (onEpProgress, finalizeEps) = Ui.ShowEpTable(epList);

var unregistered = eps.Where(ep => !ep.IsRegistered).Select(ep => ep.Name).ToArray();
if (unregistered.Length > 0)
{
    var result = await manager.DownloadAndRegisterEpsAsync(
        unregistered,
        (epName, percent) => onEpProgress(epName, percent));
    finalizeEps(result.FailedEps?.ToArray());
}

// ── Browse model catalog & pick a model ──────────────────────────────────

Ui.Section("Model Catalog");

var catalog = await manager.GetCatalogAsync();
var models = (await catalog.ListModelsAsync()).ToList();

// Cache state moved to variant-level APIs in newer SDK versions.
var modelCachedRank = new Dictionary<string, int>(StringComparer.Ordinal);
foreach (var m in models)
{
    bool anyVariantCached = false;
    foreach (var v in m.Variants)
    {
        if (await v.IsCachedAsync())
        {
            anyVariantCached = true;
            break;
        }
    }

    modelCachedRank[m.Alias] = anyVariantCached ? 0 : 1;
}

models.Sort((a, b) =>
{
    int ca = modelCachedRank[a.Alias], cb = modelCachedRank[b.Alias];
    if (ca != cb) return ca.CompareTo(cb);
    return string.Compare(a.Alias, b.Alias, StringComparison.Ordinal);
});

var catalogRows = new List<Ui.CatalogRow>();
int num = 1;
for (int i = 0; i < models.Count; i++)
{
    var m = models[i];
    var sizeGb = m.Info?.FileSizeMb > 0 ? $"{m.Info.FileSizeMb / 1024.0:F1}" : "?";
    var task = m.Info?.Task ?? "?";

    for (int v = 0; v < m.Variants.Count; v++)
    {
        var variant = m.Variants[v];
        bool variantCached = await variant.IsCachedAsync();
        catalogRows.Add(new Ui.CatalogRow(
            ModelIdx: i,
            VariantIdx: v,
            Alias: m.Alias,
            VariantId: variant.Id,
            SizeGb: sizeGb,
            Task: task,
            IsCached: variantCached
        ));
        num++;
    }
}

Ui.ShowCatalog(catalogRows, models.Count);

var choiceStr = Ui.AskUser($"\n  Select a model [\x1b[36m1-{catalogRows.Count}\x1b[0m]: ");
if (!int.TryParse(choiceStr, out int selectedIdx) || selectedIdx < 1 || selectedIdx > catalogRows.Count)
{
    Console.WriteLine("  Invalid selection.");
    return;
}
selectedIdx--;

var chosen = catalogRows[selectedIdx];
Console.WriteLine($"\n  Selected: \x1b[32m{chosen.Alias}\x1b[0m ({chosen.VariantId})");

// ── Download & load the model ────────────────────────────────────────────

var model = await catalog.GetModelAsync(chosen.Alias)
    ?? throw new Exception($"Model '{chosen.Alias}' not found.");
var chosenVariant = model.Variants[chosen.VariantIdx];
model.SelectVariant(chosenVariant);

Ui.Section($"Model – {chosen.Alias}");

if (!await chosenVariant.IsCachedAsync())
{
    var (onDlProgress, finalizeDl) = Ui.CreateDownloadBar(chosen.Alias);
    await model.DownloadAsync(progress => onDlProgress(progress));
    finalizeDl();
}

await model.LoadAsync();
Console.WriteLine("  \x1b[32m✓\x1b[0m Model loaded\n");

// ── Detect task type ─────────────────────────────────────────────────────

var taskType = (model.Info?.Task ?? "").ToLowerInvariant();
var isAudio = taskType.Contains("speech-recognition") ||
              taskType.Contains("speech-to-text") ||
              chosen.Alias.Contains("whisper", StringComparison.OrdinalIgnoreCase);

if (isAudio)
{
    // ── Audio Transcription ──────────────────────────────────────────────

    Ui.Section("Audio Transcription  (enter a file path, /quit to exit)");

    var audioClient = await model.GetAudioClientAsync();

    while (true)
    {
        var input = Ui.AskUser("  \x1b[36maudio file> \x1b[0m");
        var trimmed = input?.Trim() ?? "";
        if (string.IsNullOrEmpty(trimmed)) continue;
        if (trimmed is "/quit" or "/exit" or "/q") break;

        var audioPath = Path.GetFullPath(trimmed);
        Console.WriteLine($"  {audioPath}\n");

        var box = new Ui.StreamBox();
        try
        {
            await foreach (var chunk in audioClient.TranscribeAudioStreamingAsync(
                audioPath, CancellationToken.None))
            {
                if (!string.IsNullOrEmpty(chunk.Text))
                {
                    foreach (var c in chunk.Text) box.Write(c);
                }
            }
        }
        catch (Exception ex)
        {
            box.Finish();
            Console.WriteLine($"  \x1b[31mError: {ex.Message}\x1b[0m\n");
            continue;
        }
        box.Finish();
    }
}
else
{
    // ── Interactive Chat ─────────────────────────────────────────────────

    Ui.Section("Chat  (type a message, /quit to exit)");

    var chatClient = await model.GetChatClientAsync();
    var messages = new List<ChatMessage>
    {
        ChatMessage.FromSystem("You are a helpful assistant."),
    };

    while (true)
    {
        var input = Ui.AskUser();
        var trimmed = input?.Trim() ?? "";
        if (string.IsNullOrEmpty(trimmed)) continue;
        if (trimmed is "/quit" or "/exit" or "/q") break;

        Console.Write("\x1b[1A\r\x1b[K");
        Ui.PrintUserMsg(trimmed);

        messages.Add(ChatMessage.FromUser(trimmed));

        var box = new Ui.StreamBox();
        var response = "";

        await foreach (var chunk in chatClient.CompleteChatStreamingAsync(
            messages.ToArray(), CancellationToken.None))
        {
            var content = chunk.Choices?[0]?.Delta?.Content;
            if (!string.IsNullOrEmpty(content))
            {
                response += content;
                foreach (var c in content) box.Write(c);
            }
        }
        box.Finish();

        messages.Add(ChatMessage.FromAssistant(response));
    }
}

// ── Clean up ─────────────────────────────────────────────────────────────

await model.UnloadAsync();
manager.Dispose();
