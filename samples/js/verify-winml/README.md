# Verify WinML 2.0 Execution Providers (JavaScript)

This sample verifies that WinML 2.0 execution providers are correctly discovered,
downloaded, and registered using the Foundry Local JavaScript SDK. It uses registered
WinML EP-backed model variants and finishes with one native streaming chat check.

## Prerequisites

- Windows with a compatible GPU
- Node.js 20+

## Setup

`package.json` installs `foundry-local-sdk` using the `latest` npm tag.
The SDK's installer handles the WinML runtime dependencies automatically:

```bash
npm install
```

## Run

```bash
node app.js
```
