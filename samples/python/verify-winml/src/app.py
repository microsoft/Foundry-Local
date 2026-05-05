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
    return rt is not None and rt.device_type in ("GPU", "NPU")


def variant_score(variant) -> int:
    model_id = variant.id.lower()
    rt = variant.info.runtime

    score = 10000 if rt and rt.device_type == "NPU" else 0
    if "whisper" in model_id:
        score += 5000
    if "reasoning" in model_id or "deepseek-r1" in model_id or "gpt-oss" in model_id:
        score += 2000

    if "0.5b" in model_id:
        score += 0
    elif "1.5b" in model_id:
        score += 100
    elif "3b" in model_id:
        score += 300
    elif "7b" in model_id:
        score += 700
    elif "14b" in model_id:
        score += 1400
    elif "20b" in model_id:
        score += 2000
    else:
        score += 500

    return score


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

    if not accelerated_variants:
        print(f"\n{FAIL} No accelerated model variants are available.")
        print(f"{WARN} Ensure the system has a compatible accelerator and matching model variants installed.")
        _print_summary()
        return

    # ── 3. Download & Load Model ──────────────────────────────
    print_separator("Step 3: Download & Load Model")

    chosen = None
    downloaded_any = False
    last_load_error = None
    candidate_variants = sorted(accelerated_variants, key=variant_score)
    for candidate in candidate_variants:
        chosen_ep = candidate.info.runtime.execution_provider if candidate.info.runtime else "unknown"
        print(f"\n{INFO} Trying model: {candidate.id} (EP: {chosen_ep})")

        try:
            def dl_progress(percent):
                print(f"\r  Downloading model: {percent:.1f}%", end="", flush=True)

            candidate.download(progress_callback=dl_progress)
            print()
            downloaded_any = True
        except Exception as e:
            print()
            print(f"{WARN} Skipping {candidate.id}: download failed: {e}")
            last_load_error = e
            continue

        try:
            candidate.load()
            chosen = candidate
            break
        except Exception as e:
            print(f"{WARN} Skipping {candidate.id}: load failed: {e}")
            last_load_error = e

    log_result("Model Download", downloaded_any,
               "At least one accelerated variant downloaded" if downloaded_any
               else str(last_load_error) if last_load_error else "No accelerated variant could be downloaded")

    if chosen is None:
        log_result("Model Load", False,
                   str(last_load_error) if last_load_error else "No accelerated variant could be loaded on this machine")
        _print_summary()
        return

    log_result("Model Load", True, f"Loaded {chosen.id}")

    # ── 4. Streaming Chat Completions (Native SDK) ────────────
    print_separator("Step 4: Streaming Chat Completions (Native)")
    messages = [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is 2 + 2? Reply with just the number."},
    ]

    try:
        client = chosen.get_chat_client()
        client.settings.temperature = 0
        client.settings.max_tokens = 16
        response_text = ""
        start = time.time()
        for chunk in client.complete_streaming_chat(messages):
            choices = getattr(chunk, "choices", None)
            content = choices[0].delta.content if choices and len(choices) > 0 else None
            if content:
                response_text += content
                print(content, end="", flush=True)
        elapsed = time.time() - start
        print()
        log_result("Streaming Chat (Native)", len(response_text) > 0,
                   f"{len(response_text)} chars in {elapsed:.2f}s")
    except Exception as e:
        log_result("Streaming Chat (Native)", False, str(e))

    try:
        chosen.unload()
        print(f"{INFO} Model unloaded.")
    except Exception as e:
        print(f"{WARN} Failed to unload model: {e}")

    _print_summary()


def _print_summary():
    print_separator("Summary")
    passed = sum(1 for _, p in results if p)
    total = len(results)
    for name, p in results:
        print(f"  {'PASS' if p else 'FAIL'} {name}")
    print(f"\n  {passed}/{total} tests passed")
    if passed < total:
        sys.exit(1)


if __name__ == "__main__":
    main()
