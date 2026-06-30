using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

// ---------------------------------------------------------------------------
// Private Catalog sample — register a customer MDS catalog with a self-signed
// JWT, list public + private models, and run a streaming chat completion.
//
// Required:
//   --customer <name>     Customer name (or env MDS_CUSTOMER)
//   --key-dir <path>      Directory with <customer>-key.pem (or env MDS_KEY_DIR)
//
// Optional:
//   --model <name>        Alias or variant id (otherwise interactive picker)
//   --prompt <text>       Prompt (default "Why is the sky blue?")
//   --list                List models and exit
//   --no-private          Skip private-catalog registration (public only)
//   --show-uri            Print variant URIs alongside the listing
// ---------------------------------------------------------------------------

string? mdsCustomer = Environment.GetEnvironmentVariable("MDS_CUSTOMER");
string? mdsKeyDir = Environment.GetEnvironmentVariable("MDS_KEY_DIR");
// MDS endpoint. Core hard-codes this same prod endpoint inside AddCatalogAsync;
// the sample additionally calls /catalog directly to snapshot the customer's
// private model IDs (see FetchPrivateModelIdsAsync). Override with --host or
// env MDS_HOST (e.g. the staging endpoint).
string mdsHost = Environment.GetEnvironmentVariable("MDS_HOST") ?? "https://mds-web-app.azurewebsites.net";
string? cliModel = null;
string cliPrompt = "Why is the sky blue?";
bool listOnly = false;
bool noPrivate = false;
bool showUri = false;

for (int i = 0; i < args.Length; i++)
{
    string Next()
    {
        if (i + 1 >= args.Length)
        {
            Console.WriteLine($"Error: {args[i]} requires a value.");
            Environment.Exit(1);
        }
        return args[++i];
    }

    switch (args[i])
    {
        case "-c": case "--customer": mdsCustomer = Next(); break;
        case "--key-dir":             mdsKeyDir = Next(); break;
        case "--host":                mdsHost = Next(); break;
        case "-m": case "--model":    cliModel = Next(); break;
        case "-p": case "--prompt":   cliPrompt = Next(); break;
        case "-l": case "--list":     listOnly = true; break;
        case "--no-private":          noPrivate = true; break;
        case "--show-uri":            showUri = true; break;
        case "-h": case "--help":     PrintUsage(); return;
        default:
            Console.WriteLine($"Unknown argument: {args[i]}");
            PrintUsage();
            return;
    }
}

if (string.IsNullOrWhiteSpace(mdsCustomer))
{
    Console.WriteLine("Error: --customer <name> (or MDS_CUSTOMER) is required.");
    PrintUsage();
    return;
}
if (string.IsNullOrWhiteSpace(mdsKeyDir))
{
    Console.WriteLine("Error: --key-dir <path> (or MDS_KEY_DIR) is required.");
    PrintUsage();
    return;
}

// --- Derive customer resources (same convention as mds/scripts/onboard.py) ---
var customerLower = mdsCustomer.ToLowerInvariant();
var safeName = customerLower.Replace(" ", "").Replace("-", "");
var registryName = $"mds-{customerLower}-registry";
var issuer = $"https://mds{safeName}jwks.blob.core.windows.net/jwks";
var kid = $"mds-{customerLower}-key-1";
var keyPath = Path.Combine(mdsKeyDir, $"{customerLower}-key.pem");

if (!File.Exists(keyPath))
{
    Console.WriteLine($"Error: Private key not found at {keyPath}");
    Console.WriteLine("Run: python mds/scripts/onboard.py --customer <name> --test-keys");
    return;
}

var jwt = SignJwt(keyPath, kid, issuer, registryName);
Console.WriteLine($"Signed JWT for '{mdsCustomer}' (registry={registryName})");

// --- Init Foundry Local ---
await FoundryLocalManager.CreateAsync(
    new Configuration
    {
        AppName = "private_catalog_sample",
        LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information,
    },
    Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

// --- Register private catalog (falls back to public-only on failure) ---
var catalog = await mgr.GetCatalogAsync();
bool privateRegistered = false;

if (noPrivate)
{
    Console.WriteLine("\n[--no-private] Skipping AddCatalogAsync.");
}
else
{
    Console.WriteLine("\nRegistering private catalog...");
    try
    {
        await catalog.AddCatalogAsync("private", new PrivateCatalogOptions
        {
            BearerToken = jwt,
            Audience = "model-distribution-service",
        });
        privateRegistered = true;
        Console.WriteLine("Private catalog registered.");
    }
    catch (Exception ex) when (ex is not OperationCanceledException)
    {
        Console.WriteLine($"Warning: could not register private catalog ({ex.Message}).");
        Console.WriteLine("Continuing with public catalog only.");
    }
}

// --- List models grouped by origin ---
// Classify private vs public by membership in an authoritative snapshot of
// model IDs taken directly from MDS at registration time.
//
// Why not classify by Uri alone? Once a model is downloaded, neutron can
// rewrite its catalog entry to a "BYO Local" stub (uri=local://<name>,
// providerType=Local) if the cached id doesn't match anything currently
// returned by the live catalogs. The original source-catalog information is
// then lost, so a Uri-substring heuristic misclassifies cached private models
// as public. The MDS snapshot is the source of truth; ProviderType and Uri are
// fallbacks for any entry the snapshot didn't cover.
var privateIds = privateRegistered
    ? await FetchPrivateModelIdsAsync(mdsHost, jwt)
    : new HashSet<string>(StringComparer.OrdinalIgnoreCase);

var allModels = await catalog.ListModelsAsync();
var allVariants = allModels.SelectMany(m => m.Variants).ToList();

bool IsPrivate(IModel v) =>
    privateIds.Contains(v.Id)
    || string.Equals(v.Info.ProviderType, "AzurePrivate", StringComparison.OrdinalIgnoreCase)
    || v.Info.Uri?.Contains(registryName, StringComparison.OrdinalIgnoreCase) == true;

var publicVariants = allVariants.Where(v => !IsPrivate(v)).ToList();
var privateVariants = allVariants.Where(IsPrivate).ToList();
allVariants = publicVariants.Concat(privateVariants).ToList();

int idx = 0;
Console.WriteLine($"\n=== Public Models ({publicVariants.Count}) ===");
foreach (var v in publicVariants)
{
    Console.WriteLine($"  [{++idx}] {v.Alias} ({v.Id})");
    if (showUri) Console.WriteLine($"       uri: {v.Info.Uri}");
}
if (privateRegistered)
{
    Console.WriteLine($"\n=== Private Models ({privateVariants.Count}) ===");
    if (privateVariants.Count == 0) Console.WriteLine("  (none)");
    foreach (var v in privateVariants)
    {
        Console.WriteLine($"  [{++idx}] {v.Alias} ({v.Id})");
        if (showUri) Console.WriteLine($"       uri: {v.Info.Uri}");
    }
}

if (listOnly) return;

// --- Resolve a model (CLI or interactive) ---
string? input = cliModel;
if (string.IsNullOrWhiteSpace(input))
{
    Console.Write("\nEnter model number, alias, or variant id (q to quit): ");
    input = Console.ReadLine()?.Trim();
    if (string.IsNullOrEmpty(input) || input.Equals("q", StringComparison.OrdinalIgnoreCase)) return;
    if (int.TryParse(input, out int n) && n >= 1 && n <= allVariants.Count)
        input = allVariants[n - 1].Id;
}

var model = await ResolveModel(catalog, allVariants, input!);
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
await foreach (var chunk in chat.CompleteChatStreamingAsync(messages, CancellationToken.None))
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

await model.UnloadAsync();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void PrintUsage()
{
    Console.WriteLine("Usage: PrivateCatalog --customer <name> --key-dir <path> [options]");
    Console.WriteLine("  -c, --customer <name>    Customer name (env MDS_CUSTOMER)");
    Console.WriteLine("      --key-dir <path>     Dir with <customer>-key.pem (env MDS_KEY_DIR)");
    Console.WriteLine("      --host <url>         MDS host (env MDS_HOST)");
    Console.WriteLine("  -m, --model <name>       Model alias or variant id");
    Console.WriteLine("  -p, --prompt <text>      Prompt (default \"Why is the sky blue?\")");
    Console.WriteLine("  -l, --list               List models and exit");
    Console.WriteLine("      --no-private         Skip private-catalog registration");
    Console.WriteLine("      --show-uri           Print variant URIs in the listing");
}

static async Task<IModel?> ResolveModel(
    ICatalog catalog, List<IModel> allVariants, string input)
{
    // Exact variant id
    var model = await catalog.GetModelVariantAsync(input);
    if (model != null) return model;

    // Alias (prefer generic-cpu)
    var resolved = await catalog.GetModelAsync(input);
    if (resolved != null)
    {
        var pick = resolved.Variants.FirstOrDefault(v =>
            v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase))
            ?? resolved.Variants[0];
        return await catalog.GetModelVariantAsync(pick.Id);
    }

    // Substring match
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

// Hit the MDS /catalog endpoint directly with our JWT and return the set of
// model variant IDs (`<name>:<version>`) it owns. This is the authoritative
// list of "private" models for this customer, independent of whatever neutron
// decides to surface in ListModelsAsync after caching/downloads.
static async Task<HashSet<string>> FetchPrivateModelIdsAsync(string mdsHost, string jwt)
{
    var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    try
    {
        using var http = new HttpClient { BaseAddress = new Uri(mdsHost), Timeout = TimeSpan.FromSeconds(15) };
        http.DefaultRequestHeaders.Authorization =
            new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", jwt);

        // Page through /catalog until exhausted.
        int? skip = 0;
        while (skip is not null)
        {
            var body = JsonSerializer.Serialize(new
            {
                indexEntitiesRequest = new { pageSize = 500, skip }
            });
            using var content = new StringContent(body, Encoding.UTF8, "application/json");
            using var resp = await http.PostAsync("/catalog", content);
            if (!resp.IsSuccessStatusCode)
            {
                Console.WriteLine(
                    $"Warning: could not snapshot private model IDs ({(int)resp.StatusCode} {resp.ReasonPhrase}); " +
                    "private/public classification may be incorrect.");
                break;
            }
            using var doc = JsonDocument.Parse(await resp.Content.ReadAsStringAsync());
            var inner = doc.RootElement.GetProperty("indexEntitiesResponse");
            foreach (var entry in inner.GetProperty("value").EnumerateArray())
            {
                var name = entry.GetProperty("annotations").GetProperty("name").GetString();
                var version = entry.TryGetProperty("version", out var v) ? v.GetString() : null;
                if (!string.IsNullOrEmpty(name) && !string.IsNullOrEmpty(version))
                    ids.Add($"{name}:{version}");
            }
            skip = inner.TryGetProperty("nextSkip", out var ns) && ns.ValueKind == JsonValueKind.Number
                ? ns.GetInt32()
                : null;
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine(
            $"Warning: could not snapshot private model IDs ({ex.Message}); " +
            "private/public classification may be incorrect.");
    }
    return ids;
}
