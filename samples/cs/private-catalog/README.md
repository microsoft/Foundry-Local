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

The sample is configured entirely through command-line flags or environment
variables — there is no config file. The customer name and key directory are
required; everything else has a default.

| Flag | Env var | Default | Meaning |
|---|---|---|---|
| `--customer <name>` | `MDS_CUSTOMER` | _(required)_ | Customer name passed to `onboard.py`. Derives the registry (`mds-<customer>-registry`), JWKS URL, and key file name. |
| `--key-dir <path>` | `MDS_KEY_DIR` | _(required)_ | Folder containing `<customer>-key.pem` (typically `scripts/` in the MDS repo). |
| `--host <url>` | `MDS_HOST` | `https://mds-web-app.azurewebsites.net` | MDS endpoint. Use `https://mds-web-app-staging.azurewebsites.net` for staging. |

Other optional flags: `--model <alias\|id>`, `--prompt <text>`, `--list`,
`--no-private`, `--show-uri`. Run with `--help` for the full list.

## Run

From `samples/cs/private-catalog` (PowerShell):

```powershell
dotnet run -- --customer <name> --key-dir C:\path\to\MDS\scripts
```

Or via environment variables:

```powershell
$env:MDS_CUSTOMER = "<name>"
$env:MDS_KEY_DIR  = "C:\path\to\MDS\scripts"
dotnet run
```

## What it does

1. Reads `--customer`/`--key-dir` (or `MDS_CUSTOMER`/`MDS_KEY_DIR`) and derives
   the customer's registry, issuer, and key path.
2. Signs an RS256 JWT with claims:
   `iss`, `sub`, `aud=model-distribution-service`, `iat`, `exp`,
   `registry_name`, `entitlements={models:["*"], versions:["*"]}`.
3. Initializes Foundry Local (CPU execution provider only — no
   `DownloadAndRegisterEpsAsync` call, so the sample doesn't pull GPU/NPU EPs).
4. Calls `catalog.AddCatalogAsync("private", new PrivateCatalogOptions { BearerToken, Audience })`.
   If it fails (e.g. older neutron without this API), falls back to public-only.
5. Lists all models, partitioned into **Public** vs **Private**. Private models
   are identified from an authoritative snapshot of the customer's model IDs
   fetched directly from MDS `/catalog` (with `ProviderType` and `Uri` as
   fallbacks), because once a model is downloaded neutron can rewrite its
   catalog entry to a local stub and a Uri-only heuristic would misclassify it.
6. Prompts you to pick one, downloads it, loads it, and streams a chat
   completion.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Private key not found at ...` | `--key-dir` or customer name wrong | Check the `--customer`/`--key-dir` values; ensure `<customer>-key.pem` exists |
| `Warning: could not register private catalog (Unknown command)` | Neutron build predates `AddCatalogAsync` | Use a newer neutron; sample continues with public-only |
| `401 Invalid token issuer` | JWKS not yet published, or wrong issuer URL | Verify `https://mds<customer>jwks.blob.core.windows.net/jwks/.well-known/jwks.json` returns your key |
| Private model appears in **Public** section | Model's registry Uri is `local://...` | Re-upload with `mds/scripts/upload_model.py` so registry stores proper blob info |
| `Failed to download model` | Same as above, or SAS generation error | Check MDS logs; confirm `blob_prefix` tag on the registry entry |
| Build fails with `MSB3027` / file locked on `*.dll` | A previous `PrivateCatalog` process is still running | Close it (or `Stop-Process -Name PrivateCatalog`) and re-run `dotnet run` |

## MDS endpoints used by this sample

The MDS surface was intentionally scoped down (2026-05 API review) to the
endpoints below. All except `/health` require the customer JWT in
`Authorization: Bearer <token>`; `/catalog` also accepts an `X-API-Key` header
for customers without an OAuth2 issuer.

| Method | Path | Auth | Purpose |
|---|---|---|---|
| `GET` | `/health` | none | Liveness probe. Returns `{"status":"ok", ...}` (plus version/build info on current builds). |
| `POST` | `/catalog` | JWT or `X-API-Key` | Foundry-Local-shaped paginated catalog (`indexEntitiesRequest` -> `indexEntitiesResponse`). What `AddCatalogAsync` calls, and what this sample snapshots for private/public classification. Rate-limited 60/min. |
| `POST` | `/download?model={name}&version={ver}` | JWT | Returns a container-scoped `container_sas_url` + `prefix` for the model version; the client lists blobs under the prefix and drives parallel, resumable, chunked downloads. Pass-through models return a single `download_url`. SAS expires in ~15 min; on `403` re-call to re-mint. Rate-limited 30/min. |
| `GET` | `/models/{name}` | JWT | Foundry-Local model record for `{name}`. Optional `?version=`; latest when omitted. |
| `GET` | `/models/{name}/versions` | JWT | All versions of `{name}`, newest-first. |

> Note: `/status`, `/models` (list), `/models/sync`, `/admin/*`, and the legacy
> API-key-in-URL catalog variant were removed in the 2026-05 review and are not
> available.

```powershell
# Liveness (no auth)
curl https://mds-web-app.azurewebsites.net/health
```
