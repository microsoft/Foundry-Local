# Verify WinML 2.0 Execution Providers (JavaScript)

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered using the Foundry Local JavaScript SDK. It uses registered
WinML EP-backed model variants and finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- Node.js 18+

## Setup

`package.json` installs the repo-local `foundry-local-sdk` package and then
runs its WinML installer script, so the sample always uses the current
branch's WinML artifact pins:

```bash
npm install
```

## Run

```bash
node app.js
```
