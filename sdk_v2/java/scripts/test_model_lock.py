# Copyright (c) Microsoft Corporation. Licensed under the MIT License.
"""Pure metadata regressions: no model/cache reads, downloads, Java or native calls."""

import copy
import hashlib
import json
from pathlib import Path
import unittest

from model_lock import MARKER, select_model_inventory


ROOT = Path(__file__).resolve().parents[1]
WINDOWS = "483ce0b37c44b952a369de4257161df7ca42c8621109f20222ad1a9126f55001"
LINUX = "8d02c1ffd0c9532751ef736ea5941c0733b2219c15ec68c038063dada7e29b8a"


class ModelInventoryTests(unittest.TestCase):
    def setUp(self):
        self.base = json.loads((ROOT / "model-lock.json").read_text(encoding="utf-8"))
        self.targets = json.loads((ROOT / "model-target-lock.json").read_text(encoding="utf-8"))

    def select(self, rid="linux-x64"):
        return select_model_inventory(self.base, self.targets, rid)

    def test_both_observed_windows_targets_preserve_the_entire_legacy_tuple(self):
        self.assertEqual(WINDOWS, self.base["manifestSha256"])
        self.assertEqual(793344452, self.base["installedBytes"])
        for rid in ("win-x64", "win-arm64"):
            with self.subTest(rid=rid):
                selected = self.select(rid)
                self.assertEqual(rid, selected.pop("inventoryTarget"))
                self.assertEqual(self.base, selected)

    def test_linux_changes_only_the_observed_raw_marker_and_full_inventory_tuple(self):
        selected = self.select()
        self.assertEqual(LINUX, selected["manifestSha256"])
        self.assertEqual(793344449, selected["installedBytes"])
        common = lambda files: [item for item in files if item["name"] != MARKER]
        self.assertEqual(common(self.base["files"]), common(selected["files"]))
        self.assertEqual(15, len(common(selected["files"])))
        marker = next(item for item in selected["files"] if item["name"] == MARKER)
        self.assertEqual({"name": MARKER, "bytes": 87,
                          "sha256": "9bb2dbe6766fb9a5e3e1c8407a88141a480363d0aca7d4df4f88aa3e0399adeb"}, marker)
        self.assertEqual(self.base["id"], selected["id"])
        self.assertEqual(self.base["executionProvider"], selected["executionProvider"])
        self.assertEqual(self.base["version"], selected["version"])

    def test_selection_does_not_modify_inputs_or_share_mutable_pins(self):
        before = copy.deepcopy((self.base, self.targets))
        selected = self.select()
        selected["files"][0]["sha256"] = "0" * 64
        next(item for item in selected["files"] if item["name"] == MARKER)["bytes"] = 1
        self.assertEqual(before, (self.base, self.targets))

    def test_unobserved_targets_do_not_inherit_linux_or_windows(self):
        for rid in ("linux-arm64", "osx-arm64"):
            with self.subTest(rid=rid), self.assertRaisesRegex(ValueError, "No reviewed observed"):
                self.select(rid)

    def test_unknown_targets_and_lane_aliases_fail_closed(self):
        for rid in (None, "", "linux", "windows-x64", "macos-arm64", "osx-x64", "linux-riscv64"):
            with self.subTest(rid=rid), self.assertRaises(ValueError):
                self.select(rid)

    def test_missing_target_and_schema_version_fail_closed(self):
        for value in (None, True, 0, 2):
            with self.subTest(version=value), self.assertRaises(ValueError):
                select_model_inventory(self.base, {**self.targets, "schemaVersion": value}, "linux-x64")
        del self.targets["targets"]["linux-x64"]
        with self.assertRaises(ValueError):
            self.select()

    def test_binding_to_legacy_manifest_and_model_cannot_be_changed(self):
        for key, value in (("baseLock", "other.json"), ("baseManifestSha256", "0" * 64), ("modelId", "other:1")):
            with self.subTest(key=key), self.assertRaises(ValueError):
                select_model_inventory(self.base, {**self.targets, key: value}, "linux-x64")

    def test_immutable_payload_pin_after_marker_is_still_checked(self):
        self.base["files"][-1]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "manifest SHA256 mismatch"):
            self.select()

    def test_no_other_file_can_be_overridden(self):
        self.targets["targets"]["linux-x64"]["generatedMarker"]["name"] = "genai_config.json"
        with self.assertRaisesRegex(ValueError, "Only inference_model.json"):
            self.select()

    def test_arbitrary_marker_hash_or_size_and_aggregate_mismatch_are_rejected(self):
        for field, value in (("bytes", 88), ("bytes", True), ("bytes", -1), ("sha256", "0" * 64)):
            targets = copy.deepcopy(self.targets)
            targets["targets"]["linux-x64"]["generatedMarker"][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                select_model_inventory(self.base, targets, "linux-x64")
        for field, value in (("installedBytes", 793344452), ("manifestSha256", WINDOWS)):
            targets = copy.deepcopy(self.targets)
            targets["targets"]["linux-x64"][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                select_model_inventory(self.base, targets, "linux-x64")

    def test_complete_inventory_and_unique_ascii_names_are_required(self):
        for change in ("missing", "extra", "duplicate", "path", "unicode"):
            base = copy.deepcopy(self.base)
            if change == "missing":
                base["files"].pop()
            elif change == "extra":
                base["files"].append(copy.deepcopy(base["files"][0]))
            elif change == "duplicate":
                base["files"][-1] = copy.deepcopy(base["files"][0])
            elif change == "path":
                base["files"][-1]["name"] = "../vocab.txt"
            else:
                base["files"][-1]["name"] = "vocab\u00e9.txt"
            with self.subTest(change=change), self.assertRaises(ValueError):
                select_model_inventory(base, self.targets, "linux-x64")

    def test_ordinal_order_not_culture_or_input_order_defines_manifest(self):
        self.base["files"].reverse()
        selected = self.select()
        lines = lambda key: "".join(f"{item['name']}\t{item['bytes']}\t{item['sha256']}\n"
                                   for item in sorted(selected["files"], key=key))
        self.assertEqual(LINUX, hashlib.sha256(lines(lambda item: item["name"]).encode("utf-8")).hexdigest())
        self.assertNotEqual(LINUX, hashlib.sha256(
            lines(lambda item: item["name"].lower()).encode("utf-8")).hexdigest())


if __name__ == "__main__":
    unittest.main()
