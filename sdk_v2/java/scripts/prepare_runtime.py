# Copyright (c) Microsoft Corporation. Licensed under the MIT License.
"""Prepare pinned public native dependencies; never load code or download models."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import sys
import tempfile
import urllib.request
import zipfile


RESOURCES = Path(__file__).resolve().parents[1].joinpath(
    "src", "main", "resources", "com", "microsoft", "foundry", "local"
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def host_target():
    cpu = {"amd64": "x64", "x86_64": "x64", "aarch64": "arm64", "arm64": "arm64"}.get(
        platform.machine().lower()
    )
    system = {"Windows": "win", "Linux": "linux", "Darwin": "osx"}.get(platform.system())
    return f"{system}-{cpu}"


def read_pins(lock):
    pins = {}
    for line in (RESOURCES / lock["nativeHashes"]).read_text(encoding="ascii").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        if key in pins or not re.fullmatch(r"[0-9a-f]{64}", value):
            raise ValueError(f"Invalid or duplicate native hash: {key}")
        pins[key] = value
    return pins


def write_verified(stream, destination, expected=None):
    """Never replace an existing artifact unless its exact expected bytes match."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, suffix=".part", delete=False) as output:
        temporary = Path(output.name)
        try:
            shutil.copyfileobj(stream, output, 1024 * 1024)
            output.close()
            actual = sha256(temporary)
            if expected is not None and actual != expected:
                raise ValueError(f"SHA-256 mismatch: {destination.name}")
            if destination.exists():
                if sha256(destination) != actual:
                    raise ValueError(f"Refusing to overwrite different artifact: {destination.name}")
            else:
                os.replace(temporary, destination)
        finally:
            output.close()
            temporary.unlink(missing_ok=True)


def archive_for(package, cache, allow_download):
    archive = cache / (package["key"] + ".nupkg")
    if not archive.exists():
        if not allow_download:
            raise ValueError(f"Missing {archive.name}; network requires --explicit-download")
        print(f"Downloading {package['id']} {package['version']}", file=sys.stderr)
        with urllib.request.urlopen(package["url"], timeout=60) as response:
            write_verified(response, archive, package["sha256"])
    if sha256(archive) != package["sha256"]:
        raise ValueError(f"SHA-256 mismatch: {archive.name}; cached archives are never silently replaced")
    return archive


def prepare(args):
    lock = json.loads((RESOURCES / "runtime-lock.json").read_text(encoding="utf-8"))
    rid = args.rid or host_target()
    if rid not in lock["targets"]:
        raise ValueError(f"No pinned native artifact for {rid}")
    if not args.accept_native_licenses:
        raise ValueError("Review package licenses/notices, then pass --accept-native-licenses")
    all_pins = read_pins(lock)
    pins = {key[len(rid) + 1:]: value for key, value in all_pins.items() if key.startswith(rid + ".")}
    if len(pins) < 3:
        raise ValueError(f"Incomplete native lock for {rid}")
    archives = [(package, archive_for(package, args.cache_dir, args.explicit_download))
                for package in lock["packages"]]
    if args.verify_only:
        for name, expected in pins.items():
            if sha256(args.runtime_dir / name) != expected:
                raise ValueError(f"SHA-256 mismatch: {name}")
    else:
        written = set()
        for package, archive in archives:
            with zipfile.ZipFile(archive) as bundle:
                entries = set(bundle.namelist())
                for name, expected in pins.items():
                    source = lock["aliases"].get(name, name)
                    entry = f"runtimes/{rid}/native/{source}"
                    if entry not in entries:
                        continue
                    if name in written:
                        raise ValueError(f"Multiple packages provide {name}")
                    with bundle.open(entry) as stream:
                        write_verified(stream, args.runtime_dir / name, expected)
                    written.add(name)
                for notice in package["notices"]:
                    with bundle.open(notice) as stream:
                        write_verified(stream, args.runtime_dir / "licenses" / package["key"] / notice)
                if package["key"] == lock["header"]["package"]:
                    with bundle.open(lock["header"]["entry"]) as stream:
                        write_verified(stream, args.runtime_dir / "include" / "foundry_local" /
                                       "foundry_local_c.h", lock["header"]["sha256"])
        if written != pins.keys():
            raise ValueError(f"Missing archive entries: {sorted(pins.keys() - written)}")
        for name in ("runtime-lock.json", lock["nativeHashes"]):
            with (RESOURCES / name).open("rb") as stream:
                write_verified(stream, args.runtime_dir / name)
    print(json.dumps({"event": "runtimePrepared", "target": rid, "apiVersion": lock["apiVersion"],
                      "verifiedFiles": len(pins), "downloadAllowed": args.explicit_download}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-dir", required=True, type=Path)
    parser.add_argument("--cache-dir", required=True, type=Path)
    parser.add_argument("--rid")
    parser.add_argument("--explicit-download", action="store_true")
    parser.add_argument("--accept-native-licenses", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    try:
        prepare(args)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(json.dumps({"event": "error", "message": str(error)}), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
