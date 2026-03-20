# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import logging
import re
import shutil
import subprocess
import time

logger = logging.getLogger(__name__)


def assert_foundry_installed():
    """
    Assert that Foundry is installed.

    Raises:
        RuntimeError: If Foundry is not installed or not on PATH.
    """
    if shutil.which("foundry") is None:
        raise RuntimeError("Foundry is not installed or not on PATH!")


def get_service_uri() -> str | None:
    """
    Get the service URI if the service is running.

    Returns:
        str | None: URI of the running Foundry service, or None if not running.
    """
    with subprocess.Popen(["foundry", "service", "status"], stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
        stdout, _ = proc.communicate()
        match = re.search(r"http://(?:[a-zA-Z0-9.-]+|\d{1,3}(\.\d{1,3}){3}):\d+", stdout.decode())
        if match:
            return match.group(0)
        return None


def start_service() -> str | None:
    """
    Start the Foundry service.

    Returns:
        str | None: URI of the started Foundry service, or None if it failed to start.
    """
    if (service_url := get_service_uri()) is not None:
        logger.info("Foundry service is already running at %s", service_url)
        return service_url

    with subprocess.Popen(["foundry", "service", "start"]):
        # not checking the process output since it never finishes communication
        for _ in range(10):
            if (service_url := get_service_uri()) is not None:
                logger.info("Foundry service started successfully at %s", service_url)
                return service_url
            time.sleep(0.1)
        logger.warning("Foundry service did not start within the expected time. May not be running.")
        return None
