# Generating API Reference Docs

The `docs/api/` folder contains auto-generated markdown from the C# XML documentation comments. This guide explains how to regenerate them.

## Prerequisites

Install xmldoc2md as a global dotnet tool:

```bash
dotnet tool install -g XMLDoc2Markdown
```

## Steps

### 1. Publish the SDK

xmldoc2md needs the XML documentation file and all dependency DLLs in one folder. The project only generates the XML documentation file in **Release** mode (`-c Release`), so always publish with that configuration:

```bash
dotnet publish src/Microsoft.AI.Foundry.Local.csproj -c Release -o src/bin/publish
```

### 2. Generate the docs

```bash
dotnet xmldoc2md src/bin/publish/Microsoft.AI.Foundry.Local.dll --output docs/api --member-accessibility-level public
```

### 3. Strip compiler-generated members

Record types emit a synthetic `<Clone>$()` method that xmldoc2md includes. 

Example: 

```md
### **&lt;Clone&gt;$()**

public Runtime <Clone>$()

#### Returns

[Runtime](./microsoft.ai.foundry.local.runtime.md)<br>
```

Remove those sections:

```powershell
$cloneSection = '(?s)\r?\n### \*\*&lt;Clone&gt;\$\(\)\*\*.*'
Get-ChildItem docs/api/*.md | ForEach-Object {
  (Get-Content $_ -Raw) -replace $cloneSection | Set-Content $_
}
```

### All-in-one

```powershell
dotnet publish src/Microsoft.AI.Foundry.Local.csproj -c Release -o src/bin/publish
dotnet xmldoc2md src/bin/publish/Microsoft.AI.Foundry.Local.dll --output docs/api --member-accessibility-level public

$cloneSection = '(?s)\r?\n### \*\*&lt;Clone&gt;\$\(\)\*\*.*'
Get-ChildItem docs/api/*.md | ForEach-Object {
  (Get-Content $_ -Raw) -replace $cloneSection | Set-Content $_
}
```

## Known Limitations

xmldoc2md uses reflection metadata, which loses some C# language-level details:

- **Nullable annotations stripped** — `Task<Model?>` renders as `Task<Model>`. The `<returns>` text documents nullability, but the generated signature does not show `?`.
- **Record/init semantics lost** — Record types with `init`-only properties (e.g., `Runtime`, `ModelInfo`) are rendered with `{ get; set; }` instead of `{ get; init; }`.
- **Default parameter values omitted** — Optional parameters like `CancellationToken? ct = null` appear without their defaults.
- **Compiler-generated members surfaced** — Record types emit synthetic methods like `<Clone>$()`, `Equals(T)`, `GetHashCode()`, and `ToString()` that appear in the generated docs. These are not part of the intended public API and should be ignored.

These are cosmetic issues in the generated docs. Always refer to the source code or IntelliSense for the authoritative API surface.
