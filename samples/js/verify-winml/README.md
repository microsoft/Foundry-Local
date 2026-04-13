# Verify WinML 2.0 Execution Providers (JavaScript)

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered using the Foundry Local JavaScript SDK. It uses registered
WinML EP-backed model variants and finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Windows App SDK 2.0 runtime installed (preview1 or experimental)
- Node.js 18+

## Setup

`package.json` installs `foundry-local-sdk-winml`, which layers the WinML
preview core package onto the public JS SDK during install:

```bash
npm install
```

## Run

```bash
node app.js
```
