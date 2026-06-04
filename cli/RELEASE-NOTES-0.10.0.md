# Foundry Local CLI 0.10.0 (Preview)

First public preview of the Foundry Local CLI.

> [!IMPORTANT]
> **Preview.** This is an early build — expect rough edges, missing polish, and changes between releases. Please [file issues](https://github.com/microsoft/Foundry-Local/issues) (tag titles with `[cli]`) for anything you hit. For application integration, use the [Foundry Local SDKs](https://github.com/microsoft/Foundry-Local/tree/main/sdk).

## Coming from the older Foundry Local CLI?

The earlier service-based CLI (required to use Foundry Local before the SDKs shipped) has been replaced. Common commands map roughly as follows:

| Old (service-based CLI) | New |
|---|---|
| `foundry service start` / `stop` / `restart` / `ps` / `diag` | `foundry server start` / `stop` / `restart` / `logs` |
| `foundry cache remove` | `foundry cache rm` |
| `foundry model run <alias>` | `foundry run <alias>` |
| `foundry model info <alias>` | `foundry model show <alias>` |

Run `foundry --help` for the full surface.

## New

- `--output json` on `status`, `model list`, `model show`, `cache list`, `complete`, `transcribe`
- `-f, --force` on `foundry config set` (required for non-interactive use)
- `-f, --file <path>` on `foundry transcribe`
- `-p, --port` and `--idle-timeout` on `foundry server start`
- `foundry config reset <key>`

## Notes

- **PowerShell, UTF-8.** PowerShell's default console encoding (`ibm437`) renders the CLI's status glyphs as mojibake. Run `[Console]::OutputEncoding = [Text.Encoding]::UTF8` once per session, or add it to your `$PROFILE`.
- **Reasoning models.** For models that emit `<think>` reasoning blocks (e.g. `qwen3-0.6b`), prefer `foundry chat` or `foundry complete ... --output json`. Text-mode `complete` does not render reasoning output well in this release.

## Install

```powershell
# Windows
Add-AppxPackage .\foundry-0.10.0-win-x64-winml.msix
```

```bash
# macOS
unzip foundry-0.10.0-osx-arm64.zip -d ~/foundry-cli

# Linux
tar xzf foundry-0.10.0-linux-x64.tar.gz
```

The Linux x64 build is ~347 MB because it bundles the CUDA execution provider. All other builds are 25–110 MB.

## Verify

All binaries are signed by Microsoft Corporation. Verify hashes against `SHA256SUMS.txt`:

```powershell
Get-FileHash .\foundry-0.10.0-win-x64-winml.msix -Algorithm SHA256
```

```bash
shasum -a 256 -c SHA256SUMS.txt
```

## Feedback

File issues at <https://github.com/microsoft/Foundry-Local/issues> with the `[cli]` tag and include the output of `foundry report`.

---

## Appendix — Why the command renames

For users coming from the older service-based CLI, the rationale behind each rename:

- **`service` → `server`.** The component is an HTTP server, not a Windows service or systemd unit. `server` matches what every other OpenAI-compatible tool calls the same thing (e.g. `ollama serve`, `llama-server`, `vllm serve`).
- **`cache remove` → `cache rm`.** Shorter, and aligns with the `rm` / `del` verbs every shell already uses. `cache clear` (whole-cache) is kept distinct.
- **`model run` → `run`.** Loading and inferring is the most common reason to launch the CLI; promoting it to a top-level verb avoids the `model run model-name` stutter.
- **`model info` → `model show`.** Matches the convention used by `kubectl`, `docker`, `git remote`, and most modern CLIs, where `show` is the read verb for a single named resource.
