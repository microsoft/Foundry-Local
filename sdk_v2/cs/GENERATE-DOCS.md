# Generating API Reference Docs

The `docs/api/` folder contains auto-generated markdown from the C# XML documentation comments. This guide explains how to regenerate them.

## Prerequisites

Install xmldoc2md as a global dotnet tool:

```bash
dotnet tool install -g XMLDoc2Markdown
```

## Steps

### 1. Publish the SDK

xmldoc2md needs all dependency DLLs in one folder. A regular `dotnet build` doesn't co-locate them, so use `dotnet publish`:

```bash
dotnet publish src/Microsoft.AI.Foundry.Local.csproj -c Release -o src/bin/publish
```

### 2. Generate the docs

```bash
dotnet xmldoc2md src/bin/publish/Microsoft.AI.Foundry.Local.dll --output docs/api --member-accessibility-level public
```

### All-in-one

```powershell
dotnet publish src/Microsoft.AI.Foundry.Local.csproj -c Release -o src/bin/publish
dotnet xmldoc2md src/bin/publish/Microsoft.AI.Foundry.Local.dll --output docs/api --member-accessibility-level public
Remove-Item docs/api/index.md -Force
```

## Known Limitations

xmldoc2md uses reflection metadata, which loses some C# language-level details:

- **Nullable annotations stripped** — `Task<Model?>` renders as `Task<Model>`. The `<returns>` text documents nullability, but the generated signature does not show `?`.
- **Record/init semantics lost** — Record types with `init`-only properties (e.g., `Runtime`, `ModelInfo`) are rendered with `{ get; set; }` instead of `{ get; init; }`.
- **Default parameter values omitted** — Optional parameters like `CancellationToken? ct = null` appear without their defaults.

These are cosmetic issues in the generated docs. Always refer to the source code or IntelliSense for the authoritative API surface.
