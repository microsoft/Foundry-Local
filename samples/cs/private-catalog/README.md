# Private Catalog (C#)

End-to-end sample: register a customer MDS catalog with Foundry Local using a
self-signed RS256 JWT, list public + private models, download one, and run a
streaming chat completion.

## If you already use the public catalog

A private catalog is **the public flow plus one call**. Everything else —
`ListModelsAsync`, `DownloadAsync`, `LoadAsync`, `GetChatClientAsync` —
is identical.

```csharp
var mgr = FoundryLocalManager.Instance;
await mgr.DownloadAndRegisterEpsAsync();         // unchanged
var catalog = await mgr.GetCatalogAsync();       // unchanged

// >>> the one extra call <<<
await catalog.AddCatalogAsync("private", new Uri(mdsHost),
    options: new Dictionary<string, string>
    {
        ["BearerToken"] = jwt,         // RS256 JWT signed by your customer key
        ["Audience"]    = "model-distribution-service",
    });

var models = await catalog.ListModelsAsync();    // now includes private models
```

The JWT must carry these claims (see `SignJwt` in [Program.cs](Program.cs)):

```jsonc
{
  "iss":  "https://mds<customer>jwks.blob.core.windows.net/jwks",
  "sub":  "<your-app>",
  "aud":  "model-distribution-service",
  "iat":  <unix-ts>,
  "exp":  <unix-ts + 3600>,
  "registry_name": "mds-<customer>-registry",
  "entitlements": { "models": ["*"], "versions": ["*"] }
}
```

`ListModelsAsync` returns public **and** private variants merged. Tell them
apart by `IModel.Info.Uri`:

| URI prefix | Origin |
|---|---|
| `azureml://registries/azureml/...` | Microsoft public registry |
| `azureml://registries/mds-<customer>-registry/...` | Your private MDS catalog |
| `local://...` | Stale neutron-local registration — clean up with `foundry cache rm <model>` |

## Prerequisites

- .NET 9 SDK
- Windows x64
- A customer provisioned in MDS (registry + JWKS). Run
  `python scripts/onboard.py --customer <name> --subscription <sub> --test-keys`
  from the [MDS repo](https://github.com/coreai-microsoft/MDS) — this creates
  the resources and writes `<customer>-key.pem` into `MDS/scripts/`.
  The matching JWKS must already be published at
  `https://mds<customer>jwks.blob.core.windows.net/jwks/.well-known/jwks.json`.
- At least one model uploaded for that customer
  (`python scripts/upload_model.py --customer <name> --name <model> --path ...`).
- A running Foundry Local (`neutron`) that supports `AddCatalogAsync`.
  If it doesn't, the sample falls back to the public catalog only.

## Configure

Edit [appsettings.json](appsettings.json) with your own values:

```json
{
  "MdsHost": "https://mds-web-app.azurewebsites.net",
  "MdsCustomer": "<your-customer-name>",
  "MdsKeyDir": "C:/path/to/MDS/scripts" // where private key is stored on file 
}
```

- `MdsHost` — MDS endpoint. Prod is
  `https://mds-web-app.azurewebsites.net`; use
  `https://mds-web-app-staging.azurewebsites.net` for staging.
- `MdsCustomer` — the same name you passed to `onboard.py`. Used to derive
  the registry (`mds-<customer>-registry`), JWKS URL, and key file name.
- `MdsKeyDir` — folder containing `<customer>-key.pem` (typically
  `MDS/scripts/`).

## Run

From `samples/cs/private-catalog`:

```powershell
dotnet run
```

## What it does

1. Loads `appsettings.json` and derives the customer's registry, issuer, and
   key path.
2. Signs an RS256 JWT with claims:
   `iss`, `sub`, `aud=model-distribution-service`, `iat`, `exp`,
   `registry_name`, `entitlements={models:["*"], versions:["*"]}`.
3. Initializes Foundry Local and calls `DownloadAndRegisterEpsAsync()` so
   `ListModelsAsync` returns variants for every EP your hardware supports
   (matches `foundry model list`).
4. Calls `catalog.AddCatalogAsync("private", mdsHost, { BearerToken, Audience })`.
   If it fails (e.g. older neutron without this API), falls back to public-only.
5. Lists all models, partitioned by `Uri`:
   - **Public**: `azureml://registries/azureml/...`
   - **Private**: `azureml://registries/mds-<customer>-registry/...`
   - Anything else (e.g. `local://...`) is excluded and reported as a stale
     local registration.
6. Prompts you to pick one, downloads it, loads it, and streams a chat
   completion.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Private key not found at ...` | `MdsKeyDir` or customer name wrong | Check [appsettings.json](appsettings.json); ensure `<customer>-key.pem` exists |
| `Warning: could not register private catalog (Unknown command)` | Neutron build predates `AddCatalogAsync` | Use a newer neutron; sample continues with public-only |
| `401 Invalid token issuer` | JWKS not yet published, or wrong issuer URL | Verify `https://mds<customer>jwks.blob.core.windows.net/jwks/.well-known/jwks.json` returns your key |
| `Note: N model(s) excluded (stale local cache)` | Model was registered directly with neutron (URI `local://...`), not through MDS | Run `foundry cache rm <model>` to drop it, then re-upload via `mds/scripts/upload_model.py` if you want it back as a private model |
| `Failed to download model` | Registry entry missing blob info, or SAS generation error | Check MDS logs; confirm `blob_prefix` tag on the registry entry |
| Build fails with `MSB3027` / file locked on `*.dll` | A previous `PrivateCatalog` process is still running | Close it (or `Stop-Process -Name PrivateCatalog`) and re-run `dotnet run` |
