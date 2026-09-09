# Copyright (c) Microsoft Corporation. Licensed under the MIT License.
"""Select a reviewed external model inventory; no file, network or native operations."""

import copy
import hashlib
import re


TARGETS = ("win-x64", "win-arm64", "linux-x64", "linux-arm64", "osx-arm64")
MARKER = "inference_model.json"


def _file_entry(item):
    if not isinstance(item, dict) or set(item) != {"name", "bytes", "sha256"}:
        raise ValueError("Expected an exact file name/bytes/sha256 entry")
    if not isinstance(item["name"], str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", item["name"]):
        raise ValueError("Inventory filenames must be simple ASCII names")
    if type(item["bytes"]) is not int or item["bytes"] < 1:
        raise ValueError("Inventory sizes must be positive integer byte counts")
    if not isinstance(item["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
        raise ValueError("Inventory hashes must be lowercase SHA256")


def _check_inventory(files, expected_bytes, expected_sha256):
    if not isinstance(files, list) or len(files) != 16:
        raise ValueError("Expected the complete sixteen-file inventory")
    for item in files:
        _file_entry(item)
    names = [item["name"] for item in files]
    if len(set(name.lower() for name in names)) != len(names) or MARKER not in names:
        raise ValueError("Duplicate filenames or missing generated marker")
    if type(expected_bytes) is not int or sum(item["bytes"] for item in files) != expected_bytes:
        raise ValueError("Installed byte total does not match the complete inventory")
    # ASCII filenames make Python ordering identical to ordinal ordering, not culture sorting.
    lines = "".join(f"{item['name']}\t{item['bytes']}\t{item['sha256']}\n"
                    for item in sorted(files, key=lambda item: item["name"]))
    if hashlib.sha256(lines.encode("utf-8")).hexdigest() != expected_sha256:
        raise ValueError("Complete ordinal inventory manifest SHA256 mismatch")


def select_model_inventory(base, target_lock, rid):
    """Return a detached legacy-shaped lock for one verified native RID, or fail closed.

    Callers must pin/review the metadata revision and enforce its JSON schema.
    This checks selection/hash invariants; it does not inspect any installed file.
    """
    if rid not in TARGETS:
        raise ValueError(f"Unsupported native inventory target: {rid}")
    if type(target_lock.get("schemaVersion")) is not int or target_lock["schemaVersion"] != 1:
        raise ValueError("Unsupported target inventory schema version")
    if (target_lock.get("baseLock") != "model-lock.json" or target_lock.get("modelId") != base.get("id")
            or target_lock.get("baseManifestSha256") != base.get("manifestSha256")):
        raise ValueError("Target inventory is not bound to the supplied legacy lock")
    _check_inventory(base["files"], base["installedBytes"], base["manifestSha256"])
    targets = target_lock.get("targets")
    if not isinstance(targets, dict) or set(targets) != set(TARGETS):
        raise ValueError("Expected an explicit inventory gate for every native target")
    selected = targets[rid]
    if not isinstance(selected, dict) or selected.get("status") != "observed":
        raise ValueError(f"No reviewed observed model inventory for {rid}")
    if set(selected) != {"status", "generatedMarker", "installedBytes", "manifestSha256", "evidence"}:
        raise ValueError("Incomplete or unexpected observed inventory fields")
    if not isinstance(selected["evidence"], dict) or not selected["evidence"]:
        raise ValueError("Observed inventory needs public provenance")
    marker = selected["generatedMarker"]
    _file_entry(marker)
    if marker["name"] != MARKER:
        raise ValueError("Only inference_model.json has a target-specific pin")
    resolved = copy.deepcopy(base)
    resolved["files"] = [copy.deepcopy(marker) if item["name"] == MARKER else item
                         for item in resolved["files"]]
    resolved["installedBytes"] = selected["installedBytes"]
    resolved["manifestSha256"] = selected["manifestSha256"]
    resolved["inventoryTarget"] = rid
    _check_inventory(resolved["files"], resolved["installedBytes"], resolved["manifestSha256"])
    return resolved
