# Private Catalog (C#)

End-to-end sample: register a customer MDS catalog with Foundry Local using a
self-signed RS256 JWT, list public + private models, download one, and run a
streaming chat completion.

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
  "MdsKeyDir": "C:/path/to/MDS/scripts"
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
3. Initializes Foundry Local (CPU execution provider only — no
   `DownloadAndRegisterEpsAsync` call, so the sample doesn't pull GPU/NPU EPs).
4. Calls `catalog.AddCatalogAsync("private", mdsHost, { BearerToken, Audience })`.
   If it fails (e.g. older neutron without this API), falls back to public-only.
5. Lists all models, partitioned by `Uri`:
   - **Public**: built-in Azure ML registry
   - **Private**: `azureml://registries/mds-<customer>-registry/...`
6. Prompts you to pick one, downloads it, loads it, and streams a chat
   completion.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Private key not found at ...` | `MdsKeyDir` or customer name wrong | Check [appsettings.json](appsettings.json); ensure `<customer>-key.pem` exists |
| `Warning: could not register private catalog (Unknown command)` | Neutron build predates `AddCatalogAsync` | Use a newer neutron; sample continues with public-only |
| `401 Invalid token issuer` | JWKS not yet published, or wrong issuer URL | Verify `https://mds<customer>jwks.blob.core.windows.net/jwks/.well-known/jwks.json` returns your key |
| Private model appears in **Public** section | Model's registry Uri is `local://...` | Re-upload with `mds/scripts/upload_model.py` so registry stores proper blob info |
| `Failed to download model` | Same as above, or SAS generation error | Check MDS logs; confirm `blob_prefix` tag on the registry entry |
| Build fails with `MSB3027` / file locked on `*.dll` | A previous `PrivateCatalog` process is still running | Close it (or `Stop-Process -Name PrivateCatalog`) and re-run `dotnet run` |
