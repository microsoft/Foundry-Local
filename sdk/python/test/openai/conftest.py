# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""E2E test conftest — pre-loads ORT/GenAI DLLs before brotli is imported.

The ``_brotli`` C extension (pulled in by httpx → openai) calls
``SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)`` during
import, which restricts the DLL search path for all subsequent
``LoadLibraryExW`` calls. ORT/GenAI must be loaded BEFORE this happens.

This conftest runs as the FIRST conftest for e2e tests and pre-loads
the DLLs from ``samples/python/e2e-test-pkgs``.
"""

import ctypes
import os
import sys
from pathlib import Path


def _find_e2e_pkgs() -> Path:
    current = Path(__file__).resolve().parent
    while True:
        candidate = current / "samples" / "python" / "e2e-test-pkgs"
        if candidate.exists():
            return candidate
        parent = current.parent
        if parent == current:
            break
        current = parent
    raise FileNotFoundError("e2e-test-pkgs not found")


if sys.platform.startswith("win"):
    try:
        pkgs = _find_e2e_pkgs()
        ort_dll = pkgs / "onnxruntime.dll"
        genai_dll = pkgs / "onnxruntime-genai.dll"

        if ort_dll.exists() and genai_dll.exists():
            kernel32 = ctypes.windll.kernel32

            # Set DLL search directory BEFORE any other native imports
            kernel32.SetDllDirectoryW(str(pkgs))
            os.add_dll_directory(str(pkgs))
            os.environ["ORT_LIB_PATH"] = str(ort_dll)

            kernel32.LoadLibraryExW.restype = ctypes.c_void_p
            kernel32.LoadLibraryExW.argtypes = [
                ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_int
            ]
            _LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008

            h_ort = kernel32.LoadLibraryExW(
                str(ort_dll), None, _LOAD_WITH_ALTERED_SEARCH_PATH
            )
            h_genai = kernel32.LoadLibraryExW(
                str(genai_dll), None, _LOAD_WITH_ALTERED_SEARCH_PATH
            )

            if h_ort and h_genai:
                print(
                    f"[e2e conftest] Pre-loaded ORT ({hex(h_ort)}) "
                    f"and GenAI ({hex(h_genai)}) from {pkgs}"
                )
    except FileNotFoundError:
        pass
