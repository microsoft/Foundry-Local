#!/usr/bin/env python3
# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

"""Example: Cancelling a model download using Foundry Local Python SDK.

Demonstrates how to start a model download on a background thread and
cancel it from the main thread using ``threading.Event``.
"""

import threading
import time

from foundry_local_sdk import Configuration, FoundryLocalManager


def main():
    # 1. Initialize the SDK
    config = Configuration(app_name="CancellableDownloadExample")
    print("Initializing Foundry Local Manager")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Register execution providers
    ep_result = manager.download_and_register_eps()
    print(f"EP registration success: {ep_result.success}")

    # 2. Pick a model
    MODEL_ALIAS = "phi-4-mini"
    model = manager.catalog.get_model(MODEL_ALIAS)
    if model is None:
        print(f"Model '{MODEL_ALIAS}' not found in catalog.")
        return

    if model.is_cached:
        print(f"Model '{MODEL_ALIAS}' is already cached. Remove it first to test cancellation.")
        return

    # 3. Create a threading.Event that will signal cancellation
    cancel = threading.Event()

    # 4. Start the download on a background thread
    download_error = [None]  # mutable container for exception

    def do_download():
        try:
            model.download(
                progress_callback=lambda pct: print(f"  Download: {pct:.1f}%", end="\r"),
                cancel_event=cancel,
            )
            print("\nDownload completed successfully.")
        except Exception as e:
            download_error[0] = e

    t = threading.Thread(target=do_download)
    t.start()

    # 5. Cancel after 3 seconds
    print("Download started. Will cancel in 3 seconds...")
    time.sleep(3)
    print("\nCancelling download...")
    cancel.set()

    t.join()

    if download_error[0] is not None:
        print(f"Download was cancelled: {download_error[0]}")
    else:
        print("Download finished before cancellation took effect.")


if __name__ == "__main__":
    main()
