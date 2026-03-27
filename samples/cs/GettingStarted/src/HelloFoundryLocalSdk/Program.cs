using Microsoft.AI.Foundry.Local;
using Betalgo.Ranul.OpenAI.ObjectModels.RequestModels;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

// ---------------------------------------------------------------------------
// CLI argument parsing
// Usage:
//   .\HelloFoundryLocalSdk.exe                                  (interactive — pick from list)
//   .\HelloFoundryLocalSdk.exe --model phi-4                    (pick by alias from any catalog)
//   .\HelloFoundryLocalSdk.exe --model Phi-4-generic-cpu:1      (pick by exact variant id)
//   .\HelloFoundryLocalSdk.exe --list                           (list models and exit)
//   .\HelloFoundryLocalSdk.exe --customer cust2                 (use a different customer key)
//   .\HelloFoundryLocalSdk.exe --prompt "Hello!"                (custom prompt)
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
            Console.WriteLine("Usage: HelloFoundryLocalSdk [options]");
            Console.WriteLine();
            Console.WriteLine("Options:");
            Console.WriteLine("  -m, --model <name>       Model alias or variant id");
            Console.WriteLine("  -c, --customer <name>    Customer name (default: from appsettings MdsCustomer)");
            Console.WriteLine("  -p, --prompt <text>      Prompt to send (default: \"Why is the sky blue?\")");
            Console.WriteLine("  -l, --list               List all available models and exit");
            Console.WriteLine("  -h, --help               Show this help text");
            return;
    }
}

CancellationToken ct = new CancellationToken();

// --- Read appsettings.json for MDS config ---
var settingsPath = Path.Combine(AppContext.BaseDirectory, "appsettings.json");
var settings = JsonDocument.Parse(File.ReadAllText(settingsPath)).RootElement;
var mdsHost = settings.GetProperty("MdsHost").GetString()!;
var mdsCustomer = cliCustomer ?? settings.GetProperty("MdsCustomer").GetString()!;
var mdsKeyDir = settings.GetProperty("MdsKeyDir").GetString()!;

// --- Derive customer resources (same convention as download_model.py) ---
var safeName = mdsCustomer.ToLower().Replace(" ", "").Replace("-", "");
var registryName = $"mds-{mdsCustomer.ToLower()}-registry";
var issuer = $"https://mds{safeName}jwks.blob.core.windows.net/jwks";
var kid = $"mds-{mdsCustomer.ToLower()}-key-1";
var keyPath = Path.Combine(mdsKeyDir, $"{mdsCustomer.ToLower()}-key.pem");

if (!File.Exists(keyPath))
{
    Console.WriteLine($"Error: Private key not found at {keyPath}");
    Console.WriteLine("Run create_jwks_storage.py --customer <name> first.");
    return;
}

// --- Sign a self-service JWT (mirrors download_model.py --test) ---
var jwt = SignJwt(keyPath, kid, issuer, registryName);
Console.WriteLine($"Signed JWT for customer '{mdsCustomer}' (registry={registryName}, issuer={issuer})");

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

// Initialize Foundry Local (no Auth0 auto-connect)
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;

await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());

var catalog = await mgr.GetCatalogAsync();

// Register private catalog with the self-signed JWT bearer token
Console.WriteLine($"\nRegistering private catalog 'private' at {mdsHost}...");
await catalog.AddCatalogAsync(
    "private",
    new Uri(mdsHost),
    bearerToken: jwt,
    audience: "model-distribution-service");
Console.WriteLine("Private catalog registered.");

// Show available catalogs
var catalogNames = await catalog.GetCatalogNamesAsync();
Console.WriteLine($"\nCatalogs: {string.Join(", ", catalogNames)}");
bool hasPrivate = catalogNames.Any(n => n == "private");

// List all models (public + private)
Console.WriteLine("\n=== All Available Models (public + private) ===");
var allModels = await catalog.ListModelsAsync();
var allVariants = allModels.SelectMany(m => m.Variants).ToList();
for (int idx = 0; idx < allVariants.Count; idx++)
    Console.WriteLine($"  [{idx + 1}] {allVariants[idx].Alias} ({allVariants[idx].Id})");

// Show private catalog models if available
ModelVariant? model = null;

if (hasPrivate)
{
    Console.WriteLine("\n=== Private Catalog Models ===");
    try
    {
        await catalog.SelectCatalogAsync("private");
        var privateModels = await catalog.ListModelsAsync();
        await catalog.SelectCatalogAsync(null);

        if (privateModels.Count > 0)
        {
            var privateVariants = privateModels.SelectMany(m => m.Variants).ToList();
            foreach (var pv in privateVariants)
            {
                int num = allVariants.FindIndex(v => v.Id == pv.Id) + 1;
                if (num <= 0) num = allVariants.Count + 1; // should not happen
                Console.WriteLine($"  [{num}] {pv.Alias} ({pv.Id})");
            }
        }
    }
    catch (Exception ex)
    {
        // Reset catalog filter in case SelectCatalogAsync("private") succeeded but ListModelsAsync failed
        try { await catalog.SelectCatalogAsync(null); } catch { }
        Console.WriteLine($"  (filter failed: {ex.Message})");
        // Fallback: find by MDS_MODEL env var from combined list
        var target = Environment.GetEnvironmentVariable("MDS_MODEL") ?? "";
        if (!string.IsNullOrEmpty(target))
        {
            var match = allModels.SelectMany(m => m.Variants)
                .FirstOrDefault(v => v.Id.Contains(target, StringComparison.OrdinalIgnoreCase));
            if (match != null) { model = match; Console.WriteLine($"  Found: {match.Id}"); }
        }
    }
}

// If --list was passed, stop here
if (listOnly)
{
    return;
}

// If --model was passed, resolve it from the catalog
if (model == null && !string.IsNullOrWhiteSpace(cliModel))
{
    // Try exact variant id first (e.g. "Phi-4-generic-cpu:1")
    model = await catalog.GetModelVariantAsync(cliModel);

    // Try as an alias (e.g. "phi-4") — picks the best variant via GetModelAsync
    if (model == null)
    {
        var resolved = await catalog.GetModelAsync(cliModel);
        if (resolved != null)
        {
            // Prefer generic-cpu variant if available
            var cpuVariant = resolved.Variants
                .FirstOrDefault(v => v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase));
            var pick = cpuVariant ?? resolved.Variants[0];
            model = await catalog.GetModelVariantAsync(pick.Id);
        }
    }

    // Try substring match against the full list
    if (model == null)
    {
        var match = allModels.SelectMany(m => m.Variants)
            .FirstOrDefault(v => v.Id.Contains(cliModel, StringComparison.OrdinalIgnoreCase)
                              || v.Alias.Contains(cliModel, StringComparison.OrdinalIgnoreCase));
        if (match != null)
            model = await catalog.GetModelVariantAsync(match.Id);
    }

    if (model == null)
    {
        Console.WriteLine($"\nError: Model '{cliModel}' not found. Use --list to see available models.");
        return;
    }

    Console.WriteLine($"\nSelected model via --model: {model.Id}");
}

// No --model passed and no private model selected — interactive prompt
if (model == null && string.IsNullOrWhiteSpace(cliModel))
{
    Console.WriteLine("\nEnter a model number, alias, or variant id (or 'q' to quit):");
    Console.Write("> ");
    var input = Console.ReadLine()?.Trim();

    if (string.IsNullOrEmpty(input) || input.Equals("q", StringComparison.OrdinalIgnoreCase))
    {
        Console.WriteLine("Exiting.");
        return;
    }

    // Try as a number from the list
    if (int.TryParse(input, out int choice) && choice >= 1 && choice <= allVariants.Count)
    {
        var picked = allVariants[choice - 1];
        model = await catalog.GetModelVariantAsync(picked.Id);
    }
    else
    {
        // Try exact variant id
        model = await catalog.GetModelVariantAsync(input);

        // Try as alias
        if (model == null)
        {
            var resolved = await catalog.GetModelAsync(input);
            if (resolved != null)
            {
                var cpuVariant = resolved.Variants
                    .FirstOrDefault(v => v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase));
                var pick = cpuVariant ?? resolved.Variants[0];
                model = await catalog.GetModelVariantAsync(pick.Id);
            }
        }

        // Try substring match
        if (model == null)
        {
            var match = allVariants
                .FirstOrDefault(v => v.Id.Contains(input, StringComparison.OrdinalIgnoreCase)
                                  || v.Alias.Contains(input, StringComparison.OrdinalIgnoreCase));
            if (match != null)
                model = await catalog.GetModelVariantAsync(match.Id);
        }
    }

    if (model == null)
    {
        Console.WriteLine($"Model '{input}' not found.");
        return;
    }
    Console.WriteLine($"\nSelected model: {model.Id}");
}

// Fallback to a public model if nothing was selected
if (model == null)
{
    var fallbackModel = allVariants
        .FirstOrDefault(v => v.Id.Contains("generic-cpu", StringComparison.OrdinalIgnoreCase));
    if (fallbackModel != null)
    {
        Console.WriteLine($"\nUsing public model: {fallbackModel.Id}");
        model = await catalog.GetModelVariantAsync(fallbackModel.Id)
            ?? throw new Exception($"Failed to resolve model '{fallbackModel.Id}'.");
    }
    else
    {
        throw new Exception("No compatible models found in any catalog.");
    }
}

// Download the model (skips if already cached)
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

// Get a chat client
var chatClient = await model.GetChatClientAsync();

// Create a chat message
List<ChatMessage> messages = new()
{
    new ChatMessage { Role = "user", Content = cliPrompt }
};

// Get a streaming chat completion response
Console.WriteLine("Chat completion response:");
var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
await foreach (var chunk in streamingResponse)
{
    Console.Write(chunk.Choices[0].Message.Content);
    Console.Out.Flush();
}
Console.WriteLine();

// Tidy up - unload the model
await model.UnloadAsync();

// --- Helper: sign a self-service JWT (same as download_model.py --test) ---
static string SignJwt(string pemPath, string kid, string issuer, string registryName)
{
    var pem = File.ReadAllText(pemPath);
    using var rsa = RSA.Create();
    rsa.ImportFromPem(pem);

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
            ["versions"] = new[] { "*" }
        }
    });

    var headerB64 = Base64UrlEncode(Encoding.UTF8.GetBytes(header));
    var payloadB64 = Base64UrlEncode(Encoding.UTF8.GetBytes(payload));
    var sigInput = Encoding.UTF8.GetBytes($"{headerB64}.{payloadB64}");
    var sig = rsa.SignData(sigInput, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);

    return $"{headerB64}.{payloadB64}.{Base64UrlEncode(sig)}";
}

static string Base64UrlEncode(byte[] data)
{
    return Convert.ToBase64String(data)
        .TrimEnd('=')
        .Replace('+', '-')
        .Replace('/', '_');
}
