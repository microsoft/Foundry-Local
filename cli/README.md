# Foundry Local CLI

> [!IMPORTANT]
> **Preview.** This is an early build — expect rough edges, missing polish, and changes between releases. Please [file issues](https://github.com/microsoft/Foundry-Local/issues) for anything you hit. For application integration, use the [Foundry Local SDKs](../sdk/) (C#, JavaScript, Python, Rust) — that is the supported surface.

## Install

Binaries are published as assets on the [GitHub Releases](https://github.com/microsoft/Foundry-Local/releases) page.

| Platform | Asset |
|---|---|
| Windows x64 | `foundry-<version>-win-x64-winml.msix` (recommended) or `-win-x64.msix` |
| Windows ARM64 | `foundry-<version>-win-arm64-winml.msix` (recommended) or `-win-arm64.msix` |
| macOS (Apple Silicon) | `foundry-<version>-osx-arm64.zip` |
| Linux x64 | `foundry-<version>-linux-x64.tar.gz` |
| Linux ARM64 | `foundry-<version>-linux-arm64.tar.gz` |

On Windows and macOS you can also double-click the `.msix` or `.zip` to install with the OS installer. The commands below are the scripted equivalents.

```powershell
# Windows
Add-AppxPackage .\foundry-<version>-win-x64-winml.msix
foundry --version
```

```bash
# macOS
unzip foundry-<version>-osx-arm64.zip -d ~/foundry-cli
~/foundry-cli/foundry --version

# Linux
tar xzf foundry-<version>-linux-x64.tar.gz
./foundry/foundry --version
```

## Quick start

```text
foundry status                    # daemon + hardware + execution providers
foundry model list                # available models
foundry model load qwen3-0.6b     # download (if needed) and load
foundry chat qwen3-0.6b           # interactive chat
foundry server stop               # release the daemon when done
```

Most commands accept `--output json` for scripting. Run `foundry --help` for the full command surface.

## Links

- Docs: <https://aka.ms/foundry-local-docs>
- Discord: <https://aka.ms/foundry-local-discord>
- Issues: <https://github.com/microsoft/Foundry-Local/issues> (please tag titles with `[cli]`)

## License

MIT License — see [LICENSE.txt](./LICENSE.txt).
