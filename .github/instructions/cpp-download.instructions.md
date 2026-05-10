---
description: "Use when working on DownloadManager, model caching, model downloads, IsModelCached, or debugging why a model fails to download or loads incorrectly."
applyTo: "sdk_v2/cpp/src/download/**"
---
# Download Manager

## Cache Validation

A model directory is only considered cached if **all** of these are true:
1. The directory exists
2. No `download.tmp` file is present (incomplete download marker)
3. `inference_model.json` exists — either at the directory root or in one immediate subdirectory

`inference_model.json` is the definitive proof of a completed download. It is written by `DownloadModel` Step 3 *after* successful blob download. The helper `HasInferenceModelJson()` performs this check.

**Why this matters:** Empty directories (e.g., created by a failed or interrupted download) must not be treated as cache hits. Without the `inference_model.json` check, an empty directory would cause silent failures — the model appears "cached" but has no model files.

## Model Path Resolution

The effective model path (where `genai_config.json` lives) may be at the root of the download directory or one subdirectory down (e.g., `v1/`). `ResolveModelPath()` handles this.

## Key Files

- `download_manager.cc` / `.h` — Download orchestration and cache validation
- `inference_model_writer.cc` — Writes `inference_model.json` after download
- `download_test.cc` — Unit tests including `IsModelCachedReturnsFalseForEmptyDir`
