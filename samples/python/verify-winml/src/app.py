"""
Foundry Local SDK - WinML 2.0 EP Verification Script

Verifies:
  1. Execution providers are discovered and registered
  2. Accelerated models appear in catalog after EP registration
  3. Streaming chat completions work on an accelerated model
"""

import sys
import time
from foundry_local_sdk import Configuration, FoundryLocalManager
from foundry_local_sdk.detail.model_data_types import DeviceType


PASS = "\033[92m[PASS]\033[0m"
FAIL = "\033[91m[FAIL]\033[0m"
INFO = "\033[94m[INFO]\033[0m"
WARN = "\033[93m[WARN]\033[0m"

results = []


def log_result(test_name: str, passed: bool, detail: str = ""):
    status = PASS if passed else FAIL
    msg = f"{status} {test_name}"
    if detail:
        msg += f" - {detail}"
    print(msg)
    results.append((test_name, passed))


def print_separator(title: str):
    print(f"\n{'=' * 60}")
    print(f"  {title}")
    print(f"{'=' * 60}\n")


def is_accelerated_variant(variant) -> bool:
    rt = variant.info.runtime
    return rt is not None and rt.device_type in (DeviceType.GPU, DeviceType.NPU)


def main():
    # ── 0. Initialize FoundryLocalManager ──────────────────────
    print_separator("Initialization")
    config = Configuration(app_name="verify_winml")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance
    print(f"{INFO} FoundryLocalManager initialized.")

    # ── 1. Discover & Register EPs ────────────────────────────
    print_separator("Step 1: Discover & Register Execution Providers")
    eps = []
    try:
        eps = manager.discover_eps()
        print(f"{INFO} Discovered {len(eps)} execution providers:")
        for ep in eps:
            print(f"  - {ep.name:40s}  Registered: {ep.is_registered}")
        log_result("EP Discovery", True, f"{len(eps)} EP(s) found")
    except Exception as e:
        log_result("EP Discovery", False, str(e))

    if not eps:
        detail = "No execution providers discovered on this machine"
        log_result("EP Download & Registration", False, detail)
        print(f"\n{FAIL} {detail}.")
        _print_summary()
        return

    try:
        progress_state = {"ep": None, "percent": -1.0}

        def ep_progress(ep_name: str, percent: float):
            if progress_state["ep"] is not None and (
                progress_state["ep"] != ep_name or percent < progress_state["percent"]
            ):
                print()
            progress_state["ep"] = ep_name
            progress_state["percent"] = percent
            print(f"\r  Downloading {ep_name}: {percent:.1f}%", end="", flush=True)

        result = manager.download_and_register_eps(progress_callback=ep_progress)
        if progress_state["ep"] is not None:
            print()

        print(f"{INFO} EP registration result: success={result.success}, status={result.status}")
        if result.registered_eps:
            print(f"  Registered: {', '.join(result.registered_eps)}")
        if result.failed_eps:
            print(f"  Failed:     {', '.join(result.failed_eps)}")
        download_ok = result.success
        detail = (
            f"{len(result.registered_eps)} EP(s) registered"
            if download_ok and result.registered_eps
            else result.status
        )
        log_result("EP Download & Registration", download_ok, detail)
        if not download_ok:
            _print_summary()
            return
    except Exception as e:
        print()
        log_result("EP Download & Registration", False, str(e))
        _print_summary()
        return

    # ── 2. List Models & Find Accelerated Variants ─────────────
    print_separator("Step 2: Model Catalog - Accelerated Models")
    catalog = manager.catalog
    models = catalog.list_models()
    print(f"{INFO} Total models in catalog: {len(models)}")

    accelerated_variants = []

    for model in models:
        for variant in model.variants:
            if is_accelerated_variant(variant):
                accelerated_variants.append(variant)

    print(f"{INFO} Accelerated model variants: {len(accelerated_variants)}")
    for v in accelerated_variants:
        rt = v.info.runtime
        ep = rt.execution_provider if rt else "?"
        device = rt.device_type if rt else "?"
        print(f"  - {v.id:50s}  Device: {device:3s}  EP: {ep}")

    log_result("Catalog - Accelerated models found", len(accelerated_variants) > 0,
               f"{len(accelerated_variants)} accelerated variant(s)")

    chosen = accelerated_variants[0] if accelerated_variants else None

    if not chosen:
        print(f"\n{FAIL} No accelerated model variants are available.")
        print(f"{WARN} Ensure the system has a compatible accelerator and matching model variants installed.")
        _print_summary()
        return

    chosen_ep = chosen.info.runtime.execution_provider if chosen.info.runtime else "unknown"
    print(f"\n{INFO} Selected model: {chosen.id} (EP: {chosen_ep})")

    # ── 3. Download & Load Model ──────────────────────────────
    print_separator("Step 3: Download & Load Model")
    try:
        def dl_progress(percent):
            print(f"\r  Downloading model: {percent:.1f}%", end="", flush=True)

        chosen.download(progress_callback=dl_progress)
        print()
        log_result("Model Download", True)
    except Exception as e:
        print()
        log_result("Model Download", False, str(e))
        _print_summary()
        return

    try:
        chosen.load()
        log_result("Model Load", True, f"Loaded {chosen.id}")
    except Exception as e:
        log_result("Model Load", False, str(e))
        _print_summary()
        return

    # ── 4. Streaming Chat Completions (Native SDK) ────────────
    print_separator("Step 4: Streaming Chat Completions (Native)")
    messages = [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is 2 + 2? Reply with just the number."},
    ]

    try:
        client = chosen.get_chat_client()
        response_text = ""
        start = time.time()
        for chunk in client.complete_streaming_chat(messages):
            if chunk.text:
                response_text += chunk.text
                print(chunk.text, end="", flush=True)
        elapsed = time.time() - start
        print()
        log_result("Streaming Chat (Native)", len(response_text) > 0,
                   f"{len(response_text)} chars in {elapsed:.2f}s")
    except Exception as e:
        log_result("Streaming Chat (Native)", False, str(e))

    _print_summary()


def _print_summary():
    print_separator("Summary")
    passed = sum(1 for _, p in results if p)
    total = len(results)
    for name, p in results:
        print(f"  {'✓' if p else '✗'} {name}")
    print(f"\n  {passed}/{total} tests passed")
    if passed < total:
        sys.exit(1)


if __name__ == "__main__":
    main()
