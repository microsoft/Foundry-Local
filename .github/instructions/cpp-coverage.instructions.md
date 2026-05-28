---
description: "Use when running code coverage, analyzing coverage reports, using OpenCppCoverage, or working with run_coverage.ps1 or parse_coverage.ps1."
---
# C++ Code Coverage

## Tooling

- **OpenCppCoverage** at `C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe`
- Scripts in `sdk_v2/cpp/`:
  - `run_coverage.ps1` — Runs unit + integration tests under coverage, produces Cobertura XML
  - `parse_coverage.ps1` — Merges coverage files, reports per-file and total line coverage

## Running Coverage

```powershell
cd sdk_v2/cpp
& .\run_coverage.ps1 -Config Debug
```

Coverage **must** use Debug builds. Output goes to `sdk_v2/cpp/build/Windows/Debug/coverage/`.

## Performance

The full suite takes 15–25 minutes under coverage instrumentation:
- Unit tests (~700): ~8 minutes
- Integration tests (~100): ~10–15 minutes (model loading is the bottleneck)

Model loading under Debug+coverage is 5–10× slower than RelWithDebInfo. The embeddings model (495MB) alone can take several minutes to load.

## Output Files

| File | Contents |
|------|----------|
| `coverage/unit.cov` | Unit test coverage (binary) |
| `coverage/integration.cov` | Integration test coverage (binary) |
| `coverage/combined_coverage.xml` | Merged Cobertura XML |
| `coverage/combined_html/` | HTML report |
