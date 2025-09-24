# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import pytest
from openai import OpenAI

from foundry_local.api import FoundryLocalManager
from foundry_local.logging import set_verbosity_debug

set_verbosity_debug()

MODELS = [
    "qwen2.5-0.5b",
    "qwen2.5-0.5b-instruct-generic-cpu:3",
]


def _openai_client(m: FoundryLocalManager) -> OpenAI:
    return OpenAI(base_url=m.endpoint, api_key=m.api_key)


@pytest.fixture(scope="session")
def manager_bootstrapped() -> FoundryLocalManager:
    return FoundryLocalManager(bootstrap=True)


@pytest.fixture(scope="function")
def fresh_manager_no_bootstrap() -> FoundryLocalManager:
    return FoundryLocalManager(bootstrap=False)


# Catalog & Cache (not model-specific)


def test_catalog_lists_models(manager_bootstrapped: FoundryLocalManager):
    models = manager_bootstrapped.list_catalog_models()
    assert isinstance(models, list)
    for m in models:
        assert getattr(m, "id", None)
        assert getattr(m, "alias", None)
    print(f"Found {len(models)} models in catalog: {[m.id for m in models]}")
    all_eps = {m.execution_provider for m in models}
    print(f"Found {len(all_eps)} execution providers in catalog: {all_eps}")


def test_cache_operations(manager_bootstrapped: FoundryLocalManager):
    cache_dir = manager_bootstrapped.get_cache_location()
    assert isinstance(cache_dir, str) and cache_dir
    cached = manager_bootstrapped.list_cached_models()
    assert isinstance(cached, list)


# Service lifecycle (not model-specific)


def test_service_start_stop(fresh_manager_no_bootstrap: FoundryLocalManager):
    fresh_manager_no_bootstrap.start_service()
    assert fresh_manager_no_bootstrap.is_service_running() is True
    assert fresh_manager_no_bootstrap.service_uri
    assert fresh_manager_no_bootstrap.endpoint.endswith("/v1")
    assert isinstance(fresh_manager_no_bootstrap.api_key, str)


# OpenAI integration & model mgmt


@pytest.mark.parametrize("model_id", MODELS, ids=lambda s: f"model={s}")
def test_openai_chat_stream(model_id: str):
    mgr = FoundryLocalManager(alias_or_model_id=model_id, bootstrap=True)
    resolved = mgr.get_model_info(model_id, raise_on_not_found=True)
    client = _openai_client(mgr)

    stream = client.chat.completions.create(
        model=resolved.id,
        messages=[{"role": "user", "content": "Why is the sky blue?"}],
        stream=True,
    )

    token_count = 0
    for ch in stream:
        choice = ch.choices[0]
        delta = getattr(choice, "delta", None)
        if delta is not None and getattr(delta, "content", None):
            print(delta.content, end="")
            token_count += len(delta.content)
    assert token_count > 0


# this takes too long, don't need to run every time
# @pytest.mark.parametrize("model_id", MODELS, ids=lambda s: f"model={s}")
# def test_download_force(model_id: str, manager_bootstrapped: FoundryLocalManager):
#     # force unload model first
#     manager_bootstrapped.unload_model(model_id, force=True)
#     info = manager_bootstrapped.download_model(model_id, force=True)
#     assert info.id and info.alias


@pytest.mark.parametrize("model_id", MODELS, ids=lambda s: f"model={s}")
def test_load_then_unload(model_id: str, manager_bootstrapped: FoundryLocalManager):
    manager_bootstrapped.download_model(model_id, force=False)
    info = manager_bootstrapped.load_model(model_id)
    assert info.id and info.alias
    loaded = {m.id for m in manager_bootstrapped.list_loaded_models()}
    assert info.id in loaded
    # unload with no force should keep it since it's in ttl
    manager_bootstrapped.unload_model(model_id, force=False)
    loaded_after = {m.id for m in manager_bootstrapped.list_loaded_models()}
    assert info.id in loaded_after
    # unload with force should remove it
    manager_bootstrapped.unload_model(model_id, force=True)
    loaded_final = {m.id for m in manager_bootstrapped.list_loaded_models()}
    assert info.id not in loaded_final
