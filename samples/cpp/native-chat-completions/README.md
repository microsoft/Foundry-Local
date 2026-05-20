# Native Chat Completions (C++)

A Foundry Local **C++ SDK** sample that exercises the main chat surface area of
the SDK end-to-end. Adapted from the in-tree sample at
[`sdk/cpp/sample/main.cpp`](../../../sdk/cpp/sample/main.cpp).

The sample runs four examples in sequence:

1. **Browse catalog** — list models, variants, runtime info
2. **Non-streaming chat** — `OpenAIChatClient::CompleteChat`
3. **Streaming chat** — `OpenAIChatClient::CompleteChatStreaming` (token-by-token)
4. **Tool calling** — chat with function/tool definitions and tool-call round-trip

It also discovers and downloads execution providers via
`Manager::DiscoverEps` / `Manager::DownloadAndRegisterEps` before running.

## Prerequisites

- Windows + Visual Studio 2022 (MSVC v14.42+) with the **Desktop development with C++** workload
- [CMake](https://cmake.org/) 3.20+
- [Ninja](https://ninja-build.org/)
- [vcpkg](https://github.com/microsoft/vcpkg) — clone and set `VCPKG_ROOT`:
  ```cmd
  git clone https://github.com/microsoft/vcpkg C:\vcpkg
  C:\vcpkg\bootstrap-vcpkg.bat
  setx VCPKG_ROOT C:\vcpkg
  ```
- [NuGet CLI](https://www.nuget.org/downloads) (`nuget.exe`) on `PATH` — used by
  the SDK build to fetch the Foundry Local Core and ONNX Runtime native packages.

## Build

From an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cd samples\cpp\native-chat-completions
cmake --preset x64-debug
cmake --build --preset x64-debug
```

The executable is produced at:

```
out\build\x64-debug\native-chat-completions.exe
```

## Run

With the default chat model (`qwen3.5-2b-text`):

```cmd
out\build\x64-debug\native-chat-completions.exe
```

### CLI arguments

```
native-chat-completions.exe [chat-alias]
```

| Position | Argument     | Default           |
| -------- | ------------ | ----------------- |
| 1        | `chat-alias` | `qwen3.5-2b-text` |

Example:

```cmd
:: Run with a different model
out\build\x64-debug\native-chat-completions.exe qwen2.5-0.5b
```
