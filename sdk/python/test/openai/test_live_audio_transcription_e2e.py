# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""E2E test for live audio transcription using the e2e-test-pkgs assets.

This test validates the full pipeline:
    Audio input → streaming → transcription output

Architecture:
    Python SDK → Core.dll → onnxruntime.dll / onnxruntime-genai.dll
    (Core.dll loads ORT/GenAI internally via P/Invoke)

It uses synthetic PCM audio (440 Hz sine wave) to test the session lifecycle
without requiring a microphone.

IMPORTANT: ORT/GenAI DLLs must be loaded BEFORE ``import brotli`` (pulled in
by httpx → openai). The ``_brotli`` C extension changes the Windows DLL
loader state and prevents subsequent ``LoadLibraryExW`` calls from succeeding.
This module uses ``conftest_e2e_preload`` to handle this.

Prerequisites:
    - e2e-test-pkgs must be present at samples/python/e2e-test-pkgs
    - The nemotron model must be available in e2e-test-pkgs/models/
    - Native DLLs (Core.dll, onnxruntime.dll, onnxruntime-genai.dll) must be present
"""

from __future__ import annotations

import math
import os
import struct
import sys
import threading
from pathlib import Path

import pytest

from foundry_local_sdk.openai.live_audio_transcription_types import (
    LiveAudioTranscriptionResponse,
)


def _get_e2e_test_pkgs_path() -> Path:
    """Locate the e2e-test-pkgs directory."""
    current = Path(__file__).resolve().parent
    while True:
        candidate = current / "samples" / "python" / "e2e-test-pkgs"
        if candidate.exists():
            return candidate
        parent = current.parent
        if parent == current:
            break
        current = parent
    raise RuntimeError("Could not find samples/python/e2e-test-pkgs")


def _has_e2e_assets() -> bool:
    """Check if all required E2E assets are present."""
    try:
        pkgs = _get_e2e_test_pkgs_path()
        required = [
            pkgs / "Microsoft.AI.Foundry.Local.Core.dll",
            pkgs / "onnxruntime.dll",
            pkgs / "onnxruntime-genai.dll",
            pkgs / "models" / "nemotron",
        ]
        return all(p.exists() for p in required)
    except RuntimeError:
        return False


def _generate_sine_wave_pcm(
    sample_rate: int = 16000,
    duration_seconds: float = 2.0,
    frequency: float = 440.0,
    amplitude: float = 0.5,
) -> bytes:
    """Generate synthetic PCM audio (16-bit mono sine wave)."""
    total_samples = int(sample_rate * duration_seconds)
    pcm_bytes = bytearray(total_samples * 2)  # 16-bit = 2 bytes per sample

    for i in range(total_samples):
        t = i / sample_rate
        sample = int(32767 * amplitude * math.sin(2 * math.pi * frequency * t))
        struct.pack_into("<h", pcm_bytes, i * 2, sample)

    return bytes(pcm_bytes)


# Skip the entire module if E2E assets are not available or not on Windows
pytestmark = [
    pytest.mark.skipif(
        not _has_e2e_assets(),
        reason="E2E test assets not found (samples/python/e2e-test-pkgs)",
    ),
    pytest.mark.skipif(
        not sys.platform.startswith("win"),
        reason="E2E test requires Windows (DLLs are .dll files)",
    ),
]


def _preload_and_init():
    """Pre-load DLLs from e2e-test-pkgs and initialize the SDK.

    ORT/GenAI DLLs must be loaded via ``LoadLibraryExW`` with
    ``LOAD_WITH_ALTERED_SEARCH_PATH`` BEFORE the ``_brotli`` C extension
    is imported (via httpx → openai), because ``_brotli`` changes the
    Windows DLL loader state (calls ``SetDefaultDllDirectories``) which
    prevents subsequent ``LoadLibraryExW`` calls from succeeding.

    Since the test module is imported after conftest.py (which imports
    the SDK and transitively imports brotli), we check whether ORT is
    already loaded in the process and skip the pre-load if so.
    """
    import ctypes
    from foundry_local_sdk.detail.core_interop import (
        CoreInterop,
        RequestBuffer,
        ResponseBuffer,
        StreamingRequestBuffer,
    )
    from foundry_local_sdk.detail.utils import NativeBinaryPaths, _get_ext
    from foundry_local_sdk.configuration import Configuration
    from foundry_local_sdk.foundry_local_manager import FoundryLocalManager

    pkgs = _get_e2e_test_pkgs_path()
    paths = NativeBinaryPaths(
        core=pkgs / "Microsoft.AI.Foundry.Local.Core.dll",
        ort=pkgs / "onnxruntime.dll",
        genai=pkgs / "onnxruntime-genai.dll",
    )

    kernel32 = ctypes.windll.kernel32

    # Check if ORT is already loaded (e.g. from conftest preload)
    kernel32.GetModuleHandleW.restype = ctypes.c_void_p
    kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    ort_already_loaded = bool(kernel32.GetModuleHandleW("onnxruntime.dll"))

    if not ort_already_loaded:
        # Try to pre-load. This may fail if brotli was already imported.
        kernel32.SetDllDirectoryW(str(pkgs))
        os.add_dll_directory(str(pkgs))
        os.environ["ORT_LIB_PATH"] = str(paths.ort)

        kernel32.LoadLibraryExW.restype = ctypes.c_void_p
        kernel32.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_int]
        _LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008

        h_ort = kernel32.LoadLibraryExW(str(paths.ort), None, _LOAD_WITH_ALTERED_SEARCH_PATH)
        if not h_ort:
            return None, f"Failed to load ORT (WinError {kernel32.GetLastError()}). " \
                         "Try running this test directly (not via pytest) or pre-load DLLs."

        h_genai = kernel32.LoadLibraryExW(str(paths.genai), None, _LOAD_WITH_ALTERED_SEARCH_PATH)
        if not h_genai:
            return None, f"Failed to load GenAI (WinError {kernel32.GetLastError()})"

    # Load Core.dll and set up function signatures
    CoreInterop._flcore_library = ctypes.CDLL(str(paths.core))
    lib = CoreInterop._flcore_library
    lib.execute_command.argtypes = [ctypes.POINTER(RequestBuffer), ctypes.POINTER(ResponseBuffer)]
    lib.execute_command.restype = None
    lib.free_response.argtypes = [ctypes.POINTER(ResponseBuffer)]
    lib.free_response.restype = None
    lib.execute_command_with_callback.argtypes = [
        ctypes.POINTER(RequestBuffer), ctypes.POINTER(ResponseBuffer),
        ctypes.c_void_p, ctypes.c_void_p,
    ]
    lib.execute_command_with_callback.restype = None
    lib.execute_command_with_binary.argtypes = [
        ctypes.POINTER(StreamingRequestBuffer), ctypes.POINTER(ResponseBuffer),
    ]
    lib.execute_command_with_binary.restype = None
    CoreInterop._initialized = True

    # Initialize FoundryLocalManager
    config = Configuration(
        app_name="FoundryLocalE2ETest",
        model_cache_dir=str(pkgs / "models"),
        additional_settings={"Bootstrap": "false"},
    )
    flcore_lib_name = f"Microsoft.AI.Foundry.Local.Core{_get_ext()}"
    config.foundry_local_core_path = str(paths.core_dir / flcore_lib_name)
    config.additional_settings["OrtLibraryPath"] = str(paths.ort)
    config.additional_settings["OrtGenAILibraryPath"] = str(paths.genai)

    FoundryLocalManager.instance = None
    FoundryLocalManager.initialize(config)

    return FoundryLocalManager.instance, None


@pytest.fixture(scope="module")
def e2e_manager():
    """Initialize FoundryLocalManager with e2e-test-pkgs DLLs."""
    from foundry_local_sdk.detail.core_interop import CoreInterop
    from foundry_local_sdk.foundry_local_manager import FoundryLocalManager

    CoreInterop._initialized = False
    CoreInterop._flcore_library = None
    CoreInterop._ort_library = None
    CoreInterop._genai_library = None
    FoundryLocalManager.instance = None

    try:
        mgr, error = _preload_and_init()
    except Exception as e:
        pytest.skip(f"Could not initialize: {e}")
        return

    if error:
        pytest.skip(error)
        return

    yield mgr

    # Teardown
    try:
        catalog = mgr.catalog
        for mv in catalog.get_loaded_models():
            try:
                mv.unload()
            except Exception:
                pass
    except Exception:
        pass
    FoundryLocalManager.instance = None


class TestLiveAudioTranscriptionE2E:
    """E2E test for live audio transcription with synthetic PCM audio."""

    def test_live_streaming_e2e_with_synthetic_pcm(self, e2e_manager):
        """Full E2E: push synthetic PCM audio through the real native pipeline.

        Validates: SDK → Core.dll → onnxruntime-genai StreamingProcessor.
        """
        catalog = e2e_manager.catalog
        model = catalog.get_model("nemotron")

        if model is None:
            pytest.skip("nemotron model not found in catalog")

        if not model.is_cached:
            pytest.skip("nemotron model not cached")

        model.load()

        try:
            audio_client = model.get_audio_client()
            session = audio_client.create_live_transcription_session()
            session.settings.sample_rate = 16000
            session.settings.channels = 1
            session.settings.bits_per_sample = 16

            session.start()

            # Collect results in background
            results: list[LiveAudioTranscriptionResponse] = []
            errors: list[Exception] = []

            def read_results():
                try:
                    for result in session.get_transcription_stream():
                        results.append(result)
                except Exception as ex:
                    errors.append(ex)

            read_thread = threading.Thread(target=read_results, daemon=True)
            read_thread.start()

            # Generate ~2 seconds of synthetic PCM audio (440Hz sine wave)
            pcm_bytes = _generate_sine_wave_pcm(
                sample_rate=16000, duration_seconds=2.0, frequency=440.0
            )

            # Push audio in 100ms chunks (matching typical mic callback size)
            chunk_size = 16000 // 10 * 2  # 100ms of 16-bit audio = 3200 bytes
            for offset in range(0, len(pcm_bytes), chunk_size):
                chunk = pcm_bytes[offset : offset + chunk_size]
                session.append(chunk)

            # Stop session to flush remaining audio and complete the stream
            session.stop()
            read_thread.join(timeout=30)

            # Verify no errors occurred
            assert len(errors) == 0, f"Stream errors: {errors}"

            # Verify response structure — synthetic audio may or may not
            # produce text, but response objects should be properly structured
            for result in results:
                assert result.content is not None
                assert len(result.content) > 0
                assert result.content[0].text is not None
                # text and transcript should match
                assert result.content[0].transcript == result.content[0].text

        finally:
            model.unload()
