# Private Catalog (C#)

End-to-end sample: register a customer MDS catalog with Foundry Local using a
self-signed RS256 JWT, list public + private models, download one, and run a
streaming chat completion.

## Prerequisites

- .NET 9 SDK
- Windows x64 (other RIDs work if you adjust `-r`)
- A customer provisioned in MDS (registry + JWKS). See
  [mds/docs/CUSTOMER_ONBOARDING.md](../../../../mds/docs/CUSTOMER_ONBOARDING.md).
- The customer's **private key** (`<customer>-key.pem`) available locally.
  The matching JWKS must already be published at
  `https://mds<customer>jwks.blob.core.windows.net/jwks/.well-known/jwks.json`.
- A running Foundry Local (`neutron`) that supports `AddCatalogAsync`.
  If it doesn't, the sample falls back to the public catalog only.

## Configure

Edit [appsettings.json](appsettings.json):

```json
{
  "MdsHost": "https://mds-web-app-staging.azurewebsites.net",
  "MdsCustomer": "emmanueltest1",
  "MdsKeyDir": "C:/Users/eassumang/work/mds/scripts"
}
```

- `MdsHost` — MDS endpoint (staging or prod).
- `MdsCustomer` — customer name. Used to derive the registry
  (`mds-<customer>-registry`), JWKS URL, and key file name.
- `MdsKeyDir` — folder containing `<customer>-key.pem`.

## Build

From this folder:

```powershell
dotnet build .\PrivateCatalog.csproj -r win-x64
```

> **Do not use `dotnet run`.** It rewrites DLLs in the output folder and
> breaks the private-catalog registration path in the copied neutron binaries.
> Always launch the `.exe` directly.

## Run

```powershell
.\bin\Debug\net9.0-windows10.0.26100\win-x64\PrivateCatalog.exe
```



## What it does

1. Loads `appsettings.json` and derives the customer's registry, issuer, and
   key path.
2. Signs an RS256 JWT with claims:
   `iss`, `sub`, `aud=model-distribution-service`, `iat`, `exp`,
   `registry_name`, `entitlements={models:["*"], versions:["*"]}`.
3. Initializes Foundry Local and registers execution providers.
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
| `dotnet run` seems to break things | It does — see note above | Run `.\...\PrivateCatalog.exe` directly |
