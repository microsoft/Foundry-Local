# Minimal setuptools shim that declares the cffi extension module so the
# produced wheel is tagged with the correct cp<ver>-cp<ver>-<platform> ABI.
#
# All project metadata lives in pyproject.toml; this file only carries the
# one piece of configuration (``cffi_modules``) that setuptools accepts
# exclusively through ``setup()`` keyword arguments.
#
# At wheel-build time setuptools+cffi import the referenced module, call
# ``ffi.compile()``, and emit ``_cffi_bindings.<abi>.<plat>.{pyd,so}`` next
# to it — exactly where ``lib_loader.py`` expects to find it.

from setuptools import setup

setup(
    cffi_modules=["src/foundry_local_sdk/_native/build_cffi.py:ffi"],
    # Tag the wheel `cp311-abi3-<plat>` so a single compiled extension works
    # on every CPython >= 3.11. The actual abi3 build is enabled by
    # py_limited_api=True / Py_LIMITED_API=0x030B0000 in build_cffi.py.
    options={"bdist_wheel": {"py_limited_api": "cp311"}},
)
