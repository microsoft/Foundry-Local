using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

// ---------------------------------------------------------------------------
// Private Catalog sample — registers a customer MDS catalog with a self-signed
// JWT, lists models (public + private), lets you pick one, and runs a streaming
// chat completion.
//
// Usage:
//   PrivateCatalog                                  (interactive — pick from list)
//   PrivateCatalog --model phi-4                    (pick by alias)
//   PrivateCatalog --model Phi-4-generic-cpu:1      (pick by exact variant id)
//   PrivateCatalog --list                           (list models and exit)
//   PrivateCatalog --customer cust2                 (override MdsCustomer)
//   PrivateCatalog --prompt "Hello!"                (custom prompt)
// ---------------------------------------------------------------------------
string? cliModel = null;
string cliPrompt = "Why is the sky blue?";
bool listOnly = false;
string? cliCustomer = null;

for (int i = 0; i < args.Length; i++)
{
    switch (args[i])
    {
        case "-m":
        case "--model":
            if (i + 1 < args.Length) cliModel = args[++i];
            else { Console.WriteLine("Error: --model requires a value."); return; }
            break;
        case "-p":
        case "--prompt":
            if (i + 1 < args.Length) cliPrompt = args[++i];
            else { Console.WriteLine("Error: --prompt requires a value."); return; }
            break;
        case "-c":
        case "--customer":
            if (i + 1 < args.Length) cliCustomer = args[++i];
            else { Console.WriteLine("Error: --customer requires a value."); return; }
            break;
        case "-l":
        case "--list":
            listOnly = true;
            break;
        case "-h":
        case "--help":
            Console.WriteLine("Usage: PrivateCatalog [options]");
            Console.WriteLine("  -m, --model <name>       Model alias or variant id");
            Console.WriteLine("  -c, --customer <name>    Customer name (default: from appsettings)");
            Console.WriteLine("  -p, --prompt <text>      Prompt (default: \"Why is the sky blue?\")");
            Console.WriteLine("  -l, --list               List models and exit");
            return;
    }
}

CancellationToken ct = default;

// --- Load config ---
var settings = JsonDocument.Parse(
    File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "appsettings.json"))).RootElement;
var mdsHost = settings.GetProperty("MdsHost").GetString()!;
var mdsCustomer = cliCustomer ?? settings.GetProperty("MdsCustomer").GetString()!;
var mdsKeyDir = settings.GetProperty("MdsKeyDir").GetString()!;

// --- Derive customer resources (same convention as mds/scripts/download_model.py) ---
var safeName = mdsCustomer.ToLower().Replace(" ", "").Replace("-", "");
var registryName = $"mds-{mdsCustomer.ToLower()}-registry";
var issuer = $"https://mds{safeName}jwks.blob.core.windows.net/jwks";
var kid = $"mds-{mdsCustomer.ToLower()}-key-1";
var keyPath = Path.Combine(mdsKeyDir, $"{mdsCustomer.ToLower()}-key.pem");

if (!File.Exists(keyPath))
{
    Console.WriteLine($"Error: Private key not found at {keyPath}");
    Console.WriteLine("Run mds/scripts/create_jwks_storage.py --customer <name> first.");
    return;
}

var jwt = SignJwt(keyPath, kid, issuer, registryName);
Console.WriteLine($"Signed JWT for '{mdsCustomer}' (registry={registryName})");

// --- Init Foundry Local ---
await FoundryLocalManager.CreateAsync(
    new Configuration { AppName = "private_catalog_sample", LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information },
    Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;
Console.WriteLine("Registering execution providers...");
await mgr.DownloadAndRegisterEpsAsync();
Console.WriteLine("Done.");

// --- Register private catalog (falls back to public-only if it fails) ---
var catalog = await mgr.GetCatalogAsync();

Console.WriteLine($"\nRegistering private catalog at {mdsHost}...");
bool privateRegistered = false;
try
{
    await catalog.AddCatalogAsync("private", new Uri(mdsHost),
        options: new Dictionary<string, string>
        {
            ["BearerToken"] = jwt,
            ["Audience"] = "model-distribution-service",
        });
    privateRegistered = true;
    Console.WriteLine("Private catalog registered.");
}
catch (Exception ex)
{
    Console.WriteLine($"Warning: could not register private catalog ({ex.Message}).");
    Console.WriteLine("Continuing with the public catalog only.");
}

// --- List models (grouped by origin) ---
// Classify by the model's Uri: private MDS models have an
// `azureml://registries/<mds-registry>/...` Uri, public ones point to the
// built-in Azure ML registry. This is robust to neutron persisting
// registered catalogs across runs (which would break a pre-snapshot approach).
var allModels = await catalog.ListModelsAsync();
var allVariants = allModels.SelectMany(m => m.Variants).ToList();

bool IsPrivate(IModel v) =>
    v.Info.Uri?.Contains(registryName, StringComparison.OrdinalIgnoreCase) == true;

var publicVariants = allVariants.Where(v => !IsPrivate(v)).ToList();
var privateVariants = allVariants.Where(IsPrivate).ToList();

// Rebuild in display order (public first, then private) so numbered selection
// in the interactive picker maps 1:1 to what's printed.
allVariants = publicVariants.Concat(privateVariants).ToList();

int idx = 0;
Console.WriteLine($"\n=== Public Models ({publicVariants.Count}) ===");
foreach (var v in publicVariants)
    Console.WriteLine($"  [{++idx}] {v.Alias} ({v.Id})");

if (privateRegistered)
{
    Console.WriteLine($"\n=== Private Models ({privateVariants.Count}) ===");
    if (privateVariants.Count == 0)
        Console.WriteLine("  (none)");
    foreach (var v in privateVariants)
        Console.WriteLine($"  [{++idx}] {v.Alias} ({v.Id})");
}

if (listOnly) return;

// --- Resolve a model (from --model or interactive prompt) ---
IModel? model = null;
string? input = cliModel;

if (string.IsNullOrWhiteSpace(input))
{
    Console.Write("\nEnter model number, alias, or variant id (q to quit): ");
    input = Console.ReadLine()?.Trim();
    if (string.IsNullOrEmpty(input) || input.Equals("q", StringComparison.OrdinalIgnoreCase)) return;

    if (int.TryParse(input, out int n) && n >= 1 && n <= allVariants.Count)
        input = allVariants[n - 1].Id;
}

model = await ResolveModel(catalog, allVariants, input!);
if (model == null)
{
    Console.WriteLine($"\nModel '{input}' not found.");
    return;
}
Console.WriteLine($"\nSelected: {model.Id}");

// --- Download / load / chat ---
await model.DownloadAsync(p =>
{
    Console.Write($"\rDownloading: {p:F1}%");
    if (p >= 100f) Console.WriteLine();
});

Console.Write($"Loading {model.Id}...");
await model.LoadAsync();
Console.WriteLine(" done.");

var chat = await model.GetChatClientAsync();
var messages = new List<ChatMessage> { new() { Role = "user", Content = cliPrompt } };

Console.WriteLine("Chat completion:");
await foreach (var chunk in chat.CompleteChatStreamingAsync(messages, ct))
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

await model.UnloadAsync();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static async Task<IModel?> ResolveModel(
    ICatalog catalog, List<IModel> allVariants, string input)
{
    // Exact variant id
    var model = await catalog.GetModelVariantAsync(input);
    if (model != null) return model;

    // Alias (prefer generic-cpu variant)
    var resolved = await catalog.GetModelAsync(input);
    if (resolved != null)
    {
        var pick = resolved.Variants.FirstOrDefault(v =>
            v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase))
            ?? resolved.Variants[0];
        return await catalog.GetModelVariantAsync(pick.Id);
    }

    // Substring match against the combined list
    var match = allVariants.FirstOrDefault(v =>
        v.Id.Contains(input, StringComparison.OrdinalIgnoreCase) ||
        v.Alias.Contains(input, StringComparison.OrdinalIgnoreCase));
    return match != null ? await catalog.GetModelVariantAsync(match.Id) : null;
}

static string SignJwt(string pemPath, string kid, string issuer, string registryName)
{
    using var rsa = RSA.Create();
    rsa.ImportFromPem(File.ReadAllText(pemPath));

    var now = DateTimeOffset.UtcNow;
    var header = JsonSerializer.Serialize(new { alg = "RS256", typ = "JWT", kid });
    var payload = JsonSerializer.Serialize(new Dictionary<string, object>
    {
        ["iss"] = issuer,
        ["sub"] = "foundry-local-sample",
        ["aud"] = "model-distribution-service",
        ["iat"] = now.ToUnixTimeSeconds(),
        ["exp"] = now.AddHours(1).ToUnixTimeSeconds(),
        ["registry_name"] = registryName,
        ["entitlements"] = new Dictionary<string, object>
        {
            ["models"] = new[] { "*" },
            ["versions"] = new[] { "*" },
        },
    });

    var h = B64Url(Encoding.UTF8.GetBytes(header));
    var p = B64Url(Encoding.UTF8.GetBytes(payload));
    var sig = rsa.SignData(Encoding.UTF8.GetBytes($"{h}.{p}"),
        HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
    return $"{h}.{p}.{B64Url(sig)}";
}

static string B64Url(byte[] data) =>
    Convert.ToBase64String(data).TrimEnd('=').Replace('+', '-').Replace('/', '_');
