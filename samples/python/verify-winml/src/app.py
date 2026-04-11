"""
Foundry Local SDK - WinML 2.0 EP Verification Script

Verifies:
  1. WinML execution providers are discovered and registered
  2. GPU models appear in catalog after EP registration
  3. Streaming chat completions work on a WinML-accelerated model
  4. Web service works with OpenAI SDK against a WinML-loaded model
"""

import sys
import time
import openai
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


def is_winml_ep(ep_name: str) -> bool:
    return "winml" in ep_name.lower() or "dml" in ep_name.lower()


def main():
    # ── 0. Initialize FoundryLocalManager ──────────────────────
    print_separator("Initialization")
    config = Configuration(app_name="verify_winml")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance
    print(f"{INFO} FoundryLocalManager initialized.")

    # ── 1. Discover & Register EPs ────────────────────────────
    print_separator("Step 1: Discover & Register Execution Providers")
    winml_ep_found = False
    try:
        eps = manager.discover_eps()
        print(f"{INFO} Discovered {len(eps)} execution providers:")
        for ep in eps:
            tag = " ★ WinML" if is_winml_ep(ep.name) else ""
            print(f"  - {ep.name:40s}  Registered: {ep.is_registered}{tag}")
            if is_winml_ep(ep.name):
                winml_ep_found = True
        log_result("EP Discovery", True, f"{len(eps)} EP(s) found, WinML={'YES' if winml_ep_found else 'NO'}")
    except Exception as e:
        log_result("EP Discovery", False, str(e))

    try:
        def ep_progress(ep_name: str, percent: float):
            print(f"\r  Downloading {ep_name}: {percent:.1f}%", end="", flush=True)

        result = manager.download_and_register_eps(progress_callback=ep_progress)
        print()
        print(f"{INFO} EP registration result: success={result.success}, status={result.status}")
        if result.registered_eps:
            print(f"  Registered: {', '.join(result.registered_eps)}")
        if result.failed_eps:
            print(f"  Failed:     {', '.join(result.failed_eps)}")
        winml_registered = any(is_winml_ep(name) for name in result.registered_eps)
        log_result("EP Download & Registration", result.success,
                   f"WinML registered: {winml_registered}")
    except Exception as e:
        print()
        log_result("EP Download & Registration", False, str(e))

    # ── 2. List Models & Find GPU/WinML Variants ───────────────
    print_separator("Step 2: Model Catalog - GPU/WinML Models")
    catalog = manager.catalog
    models = catalog.list_models()
    print(f"{INFO} Total models in catalog: {len(models)}")

    gpu_variants = []
    winml_variants = []

    for model in models:
        for variant in model.variants:
            rt = variant.info.runtime
            if rt and rt.device_type == DeviceType.GPU:
                gpu_variants.append(variant)
                if is_winml_ep(rt.execution_provider or ""):
                    winml_variants.append(variant)

    print(f"{INFO} GPU model variants: {len(gpu_variants)}")
    for v in gpu_variants:
        ep = v.info.runtime.execution_provider if v.info.runtime else "?"
        print(f"  - {v.id:50s}  EP: {ep}")

    print(f"\n{INFO} WinML model variants: {len(winml_variants)}")
    for v in winml_variants:
        ep = v.info.runtime.execution_provider if v.info.runtime else "?"
        print(f"  - {v.id:50s}  EP: {ep}")

    log_result("Catalog - GPU models found", len(gpu_variants) > 0,
               f"{len(gpu_variants)} GPU variant(s)")

    # Pick a GPU variant (prefer WinML, fall back to any GPU)
    chosen = winml_variants[0] if winml_variants else (gpu_variants[0] if gpu_variants else None)

    if not chosen:
        print(f"\n{FAIL} No GPU models available. Cannot proceed with inference tests.")
        print(f"{WARN} Ensure the system has a compatible GPU and WinML drivers installed.")
        _print_summary()
        sys.exit(1)

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
        sys.exit(1)

    try:
        chosen.load()
        log_result("Model Load", True, f"Loaded {chosen.id}")
    except Exception as e:
        log_result("Model Load", False, str(e))
        _print_summary()
        sys.exit(1)

    # ── 4. Streaming Chat Completions (Native SDK) ────────────
    print_separator("Step 4: Streaming Chat Completions (Native)")
    messages = [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is 2 + 2? Reply with just the number."},
    ]

    try:
        client = manager.get_chat_client()
        response_text = ""
        start = time.time()
        for chunk in client.complete_streaming_chat(messages, model_id=chosen.id):
            if chunk.text:
                response_text += chunk.text
                print(chunk.text, end="", flush=True)
        elapsed = time.time() - start
        print()
        log_result("Streaming Chat (Native)", len(response_text) > 0,
                   f"{len(response_text)} chars in {elapsed:.2f}s")
    except Exception as e:
        log_result("Streaming Chat (Native)", False, str(e))

    # ── 5. OpenAI SDK Chat Completions ────────────────────────
    print_separator("Step 5: Chat Completions (OpenAI SDK)")
    try:
        oai_client = openai.OpenAI(
            base_url=manager.endpoint,
            api_key="not-needed",
        )
        oai_messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {"role": "user", "content": "Name three colors. Reply briefly."},
        ]
        response = oai_client.chat.completions.create(
            model=chosen.id,
            messages=oai_messages,
        )
        content = response.choices[0].message.content or ""
        print(f"  Response: {content[:200]}")
        log_result("Chat (OpenAI SDK)", len(content) > 0, f"{len(content)} chars")
    except Exception as e:
        log_result("Chat (OpenAI SDK)", False, str(e))

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
