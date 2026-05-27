# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""setup.py — declares cffi_modules so the cffi extension is compiled at install time."""

from setuptools import setup

setup(
    cffi_modules=["src/_native/build_cffi.py:ffi"],
    options={"bdist_wheel": {"py_limited_api": "cp311"}},
)
