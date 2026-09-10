# External model inventory contract

This is external evaluator metadata, not a new Java SDK API, runtime package,
model, or JAR. The binary source remains
`d0946a0764d9cfa4b3d684940d6d5c66165427b8`; its qualified Java 17 JAR remains
64,000 bytes with SHA-256
`bf644d3127afff912683731094821a8f6a751f003c284a9c15ddceaecebe0863`.
The executed metadata revision is
`22ebea63b07addb526a1792e0303ba2f572f444a`; pin it separately from the binary
source and this later documentation revision.

## Backward-safe files and schema

[model-lock.json](model-lock.json) is unchanged: it retains all sixteen original
Windows file pins, the 793,344,452-byte total, and the complete manifest
`483ce0b37c44b952a369de4257161df7ca42c8621109f20222ad1a9126f55001`.
Fifteen payload/configuration/license pins remain common to every reviewed
target and are stored only there.

[model-target-lock.json](model-target-lock.json) is an additive sidecar conforming
to [model-target-lock.schema.json](model-target-lock.schema.json), JSON Schema
2020-12. `schemaVersion: 1`, `baseLock`, `modelId` and `baseManifestSha256` bind it
to the legacy lock. `targets` uses exact native RIDs, **not evaluator lane names**:
`win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-arm64`.

An observed entry has exactly `status`, `generatedMarker`, `installedBytes`,
`manifestSha256`, and `evidence`. `generatedMarker` is one raw file pin
(`name`, `bytes`, lowercase `sha256`), whose name must be
`inference_model.json`. An unobserved entry is only `{"status":"unobserved"}`;
it has no guessed hashes or totals. The schema is structural, not proof of
provenance: metadata must also come from the separately reviewed immutable pin.

| Native RID | Inventory gate | Raw marker bytes | Complete installed bytes | Original sidecar provenance |
|---|---|---:|---:|---|
| `win-x64` | observed | 90 | 793344452 | Hosted inventory and ASR |
| `win-arm64` | observed | 90 | 793344452 | Hosted inventory and ASR |
| `linux-x64` | observed | 87 | 793344449 | Hosted complete-inventory diagnostic |
| `linux-arm64` | observed | 87 | 793344449 | Independent hosted complete-inventory diagnostic |
| `osx-arm64` | observed | 87 | 793344449 | Independent hosted complete-inventory diagnostic |

Windows marker SHA-256:
`881e9c5b34349dabe826cf88857835d15623c1a92a311d002812c399fe1128ef`.
Independently observed Linux x64, Linux ARM64 and macOS ARM64 marker SHA-256:
`9bb2dbe6766fb9a5e3e1c8407a88141a480363d0aca7d4df4f88aa3e0399adeb`.
Each has the complete sixteen-file manifest
`8d02c1ffd0c9532751ef736ea5941c0733b2219c15ec68c038063dada7e29b8a`.
All five inventory tuples are selectable. Their original non-Windows diagnostic
provenance remains inventory-only and stopped before transcription; those
sidecar records are unchanged. Actual ASR smoke was subsequently established on
all five targets by [run 34411280765](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765),
using these exact metadata and binary pins. See
[the current smoke summary](NATIVE_SMOKE.md#five-target-hosted-smoke) and the
[immutable report](https://github.com/jiec-msft/foundry-local/blob/76706e3f8aa84a6500936a14d31eefa3fe96ce4c/sdk_v2/java/evaluation/SECOND_MATRIX_EVALUATION.md).
Inventory acceptance alone still does not prove ASR, and unknown targets or
simulated unobserved entries still fail closed.

**Evidence boundary for that successful matrix:** the executed verifier requires
all sixteen actual raw pins, the complete manifest and byte total before
transcription; retained v3 metadata and installed totals match. Success artifacts
omitted `model-verification.json`, so their new per-file observations cannot be
independently replayed. The original diagnostic rows below are not substituted
for those missing rows, and selected expected manifests are not called newly
observed manifests.

## Mandatory selection and verification

1. Validate/pin both metadata files and the selector source. Preserve independent
   runtime, header, JAR/JNA, host, JVM and native architecture checks.
2. Use the **verified native RID** to select exactly `targets[rid]`. Missing,
   unsupported, unobserved or unknown-schema entries fail closed. Do not use an
   OS-only default, infer another architecture, search for any matching hash,
   or fall back to the Windows entry.
3. Validate the legacy lock's complete manifest and total against the sidecar's
   base binding. Copy its sixteen `files` entries, replacing **only**
   `inference_model.json` with the selected raw pin. Inherit all other fields,
   including exact model ID/version/URI/provider and the fifteen common pins.
4. Take the complete `manifestSha256` and `installedBytes` from that selected
   entry. Recompute both from the sixteen expected pins and require equality.
5. Hash all sixteen actual files as raw bytes, including the generated marker
   and files sorting after it. Require each exact size/hash, recompute the
   complete actual inventory, and require its manifest and byte total to match
   the selected tuple before ASR. Retain mismatch evidence; never learn accepted
   hashes from a failing or arbitrary observation.

The manifest is SHA-256 of UTF-8 lines sorted by **ordinal, case-sensitive
filename**, each `filename<TAB>decimal bytes<TAB>lowercase SHA256<LF>`, including
the last LF. All filenames in this contract are ASCII. Python `sorted` on these
names and .NET `StringComparer.Ordinal` agree; culture sorting and case folding
do not. These are *inventory separators*, not instructions to change any model
file's newlines. Do not normalize, rewrite, or semantically compare downloaded
JSON instead of hashing its actual bytes.

Legacy consumers ignoring the sidecar remain Windows-inventory-only; adding
metadata does not make those consumers target-aware. The Java SDK itself does
not apply this external integrity gate.

## Concrete evaluator integration

[scripts/model_lock.py](scripts/model_lock.py) provides the small pure-metadata
selector `select_model_inventory(base, target_lock, rid)`. It returns a detached
legacy-shaped lock with the selected `files`, `installedBytes`,
`manifestSha256`, and an added `inventoryTarget`. It performs no file, network,
model, native or Java operations. Example after loading the separately pinned
SDK scripts module and parsing the two schema-validated metadata documents:

```python
from model_lock import select_model_inventory

# rid is returned by the existing host/JVM/native architecture verifier.
model_lock = select_model_inventory(base_lock, target_lock, rid)
installed = verify_model(cache_dir, model_lock, evidence_path)
if installed != model_lock["installedBytes"]:
    raise ValueError("Actual complete model byte total differs from selected target")
measurement["model"]["sha256"] = model_lock["manifestSha256"]
measurement["resources"]["model_install_bytes"] = installed
```

The following integration requirements were originally written against evaluator
revision `e7fa302b54fe410d762bd47965fcb7e39d4ff1b6`. The successful matrix execution
`07e40f066997d326c189f492225bc5a7bb193c0a` now consumes the separate pins; these
remain consumer requirements, not a claim that its code is included here:

1. Add a separate immutable `metadata_git_sha` pin and verify
   `model-target-lock.json`, `model-target-lock.schema.json`, and
   `scripts/model_lock.py` against it. Keep `sdk_git_sha`, all binary-producing
   files, legacy `model-lock.json`, JAR/JNA hashes and bytecode gates bound to
   `d0946a0764d9cfa4b3d684940d6d5c66165427b8`. The current `ci_prepare.py`
   binary-source diff gate need not be weakened: all its checked SDK files are
   unchanged by this additive contract.
2. In `integration.py`, select after `verify_artifacts` returns `rid`, before
   normal preparation/inference. Pass the selected flat lock to existing
   `verify_model`, retain all-file raw mismatch collection, and compare its
   returned total. Use that same selected manifest in measurement/provenance.
3. Replace any universal `ci-lock.json` model-manifest expectation with the
   selected per-RID expectation, or explicitly retain the old field as Windows
   legacy metadata only. Record metadata revision, native RID, installed bytes
   and selected manifest without relabeling binary source provenance.
4. Add evaluator-owned regressions for all five observed selections, no fallback
   for simulated unobserved or unknown targets, wrong raw marker, wrong common
   file, wrong complete manifest/total, and metadata pin drift. Preserve existing architecture,
   explicit-download consent, licensing, cleanup and unknown-byte gates.

No evaluator source or workflow is changed here. Diagnostic collection of an
unobserved target's inventory is a separately authorized task, never an
`allow-unobserved` verification escape hatch.

## Public evidence and limitations

The Windows entries are supported by the successful Windows x64/ARM64 jobs in
[run 34396100361](https://github.com/jiec-msft/foundry-local/actions/runs/34396100361),
execution `b14d37fdc848777b1ff744a5d960e3940c3d3aae`.
Linux x64's [diagnostic run](https://github.com/jiec-msft/foundry-local/actions/runs/34398826339)
at `e7fa302b54fe410d762bd47965fcb7e39d4ff1b6` completed identify/preparation but
failed the old Windows-only external lock before ASR. Its reviewed 6,034-byte
`failure.json` reports all sixteen actual files, not just the first mismatch;
its SHA-256 and public artifact URL are pinned in the sidecar. All fifteen
non-marker entries, including those after the marker, match exactly.

Linux ARM64 and macOS ARM64 were subsequently observed independently, both
executing `b08a704a824fdfaeef33ccd7e670b90d3c404960` with the unchanged `d0946a0`
binary and diagnostic fix `e7fa302b54fe410d762bd47965fcb7e39d4ff1b6`:

| Native RID | Separate diagnostic run | Separate artifact | Completed UTC |
|---|---|---|---|
| `linux-arm64` | [34401279833](https://github.com/jiec-msft/foundry-local/actions/runs/34401279833) | [10123587055](https://github.com/jiec-msft/foundry-local/actions/runs/34401279833/artifacts/10123587055) | 2026-09-09T20:30:17Z |
| `osx-arm64` | [34401280998](https://github.com/jiec-msft/foundry-local/actions/runs/34401280998) | [10123649414](https://github.com/jiec-msft/foundry-local/actions/runs/34401280998/artifacts/10123649414) | 2026-09-09T20:31:54Z |

[Evidence-only commit 384bbe5](https://github.com/jiec-msft/foundry-local/commit/384bbe55d63265c133285b1a2a86403db3417fb2)
records each independent retrieval and all sixteen raw comparisons. Each
6,034-byte `failure.json` has SHA-256
`4c1f6c49793f28076c86753934e6e3c704e992d71e0451768f0ae2d70bdf2d45`.
The separate payloads are byte-identical; neither target was promoted merely
by copying the Linux x64 conclusion. Both identified and explicitly prepared
successfully, then failed at the old Windows marker gate with zero ASR
processes, not a native cancellation, preparation failure or native skip.

The public native writer uses a text-mode `std::ofstream` with `j.dump(2)`.
That explains the newline distinction, but all three non-Windows pins are based
on their own complete hosted observations, not source inference or analogy.
Accepting these reviewed inventories is not ASR success, and does not authorize
learning new hashes automatically from later failures.

Model/runtime redistribution remains unauthorized. Existing catalog-network,
unknown transfer-byte, license-description discrepancy and old-JBR compatibility
limitations remain in force. Installed bytes are not download bytes.

Pure metadata checks, with no Java build or native work:

```powershell
python -B -m unittest discover -s sdk_v2\java\scripts -p test_model_lock.py -v
Test-Json -Json (Get-Content sdk_v2\java\model-target-lock.json -Raw) -SchemaFile sdk_v2\java\model-target-lock.schema.json
```
