# Python wheel build (sdk_v2/python)

## 32-bit cffi build trap

Symptom: `python -m build --wheel` produces `build\temp.win32-cpython-311\...`
and fails with `__cdecl` vs `__stdcall` mismatch errors when linking
foundry_local_sdk._native._cffi_bindings.

Root cause: env vars (most often `VSCMD_ARG_TGT_ARCH=x86`, also
`VSCMD_ARG_HOST_ARCH`, `Platform`, `_PYTHON_HOST_PLATFORM`,
`DISTUTILS_USE_SDK`) leak in from a prior "x86 Native Tools Command Prompt"
invocation. setuptools' MSVC env detection prefers those over the
interpreter's bitness and silently picks `HostX86\x86\cl.exe`. Python itself
is still 64-bit when this happens — diagnosing by checking `python -c
"import struct; print(struct.calcsize('P')*8)"` is misleading.

Fix: remove those env vars before invoking `python -m build`. The script
`samples/python/test-v2.ps1` does this automatically and also asserts a
64-bit interpreter.
