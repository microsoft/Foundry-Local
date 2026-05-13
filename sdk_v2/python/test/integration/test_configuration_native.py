# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Configuration → native plumbing tests.

We do not initialize a Manager (the singleton is owned by the
``manager`` fixture and only one can exist per process). Instead we
build native handles directly and assert they are accepted by the
config vtable without raising.
"""
from __future__ import annotations

import pytest

from foundry_local_sdk import Configuration, LogLevel


@pytest.fixture(autouse=True)
def _require_native(native_api):
    return native_api


class TestBuildNative:
    def test_minimal_config_builds(self):
        c = Configuration(app_name="BuildNativeTest")
        ptr = c._build_native()
        assert ptr is not None
        # Release immediately — Manager_Create would otherwise own this.
        from foundry_local_sdk._native.api import api
        api.config.Configuration_Release(ptr)

    def test_all_directories_accepted(self, tmp_path):
        c = Configuration(
            app_name="BuildNativeTest",
            app_data_dir=str(tmp_path / "app"),
            model_cache_dir=str(tmp_path / "cache"),
            logs_dir=str(tmp_path / "logs"),
            log_level=LogLevel.WARNING,
        )
        ptr = c._build_native()
        assert ptr is not None
        from foundry_local_sdk._native.api import api
        api.config.Configuration_Release(ptr)

    def test_catalog_urls_accepted(self):
        c = Configuration(
            app_name="BuildNativeTest",
            catalog_urls=[
                ("https://example.invalid/catalog.json", None),
                ("https://example.invalid/other.json", "filter=cpu"),
            ],
        )
        ptr = c._build_native()
        assert ptr is not None
        from foundry_local_sdk._native.api import api
        api.config.Configuration_Release(ptr)

    def test_additional_settings_accepted(self):
        c = Configuration(
            app_name="BuildNativeTest",
            additional_settings={"Bootstrap": "false", "K2": "v2"},
        )
        ptr = c._build_native()
        assert ptr is not None
        from foundry_local_sdk._native.api import api
        api.config.Configuration_Release(ptr)
