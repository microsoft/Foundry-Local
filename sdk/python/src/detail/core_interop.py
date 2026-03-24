# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

from __future__ import annotations

import ctypes
import json
import logging
import os
import sys

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Optional
from ..configuration import Configuration
from ..exception import FoundryLocalException
from .utils import get_native_binary_paths, _get_ext

logger = logging.getLogger(__name__)

class InteropRequest:
    """Request payload for a Foundry Local Core command.

    Args:
        params: Dictionary of key-value string parameters.
    """

    def __init__(self, params: Dict[str, str] = None):
        self.params = params or {}

    def to_json(self) -> str:
        """Serialize the request to a JSON string."""
        return json.dumps({"Params": self.params}, ensure_ascii=False) # FLC expects UTF-8 encoded JSON (not ascii)


class RequestBuffer(ctypes.Structure):
    """ctypes Structure matching the native ``RequestBuffer`` C struct."""

    _fields_ = [
        ("Command", ctypes.c_void_p),
        ("CommandLength", ctypes.c_int),
        ("Data", ctypes.c_void_p),
        ("DataLength", ctypes.c_int),
    ]


class ResponseBuffer(ctypes.Structure):
    """ctypes Structure matching the native ``ResponseBuffer`` C struct."""

    _fields_ = [
        ("Data", ctypes.c_void_p),
        ("DataLength", ctypes.c_int),
        ("Error", ctypes.c_void_p),
        ("ErrorLength", ctypes.c_int),
    ]


@dataclass
class Response:
    """Result from a Foundry Local Core command.
    Either ``data`` or ``error`` will be set, never both.
    """

    data: Optional[str] = None
    error: Optional[str] = None


class CallbackHelper:
    """Internal helper class to convert the callback from ctypes to a str and call the python callback."""
    @staticmethod
    def callback(data_ptr, length, self_ptr):
        self = None
        try:
            self = ctypes.cast(self_ptr, ctypes.POINTER(ctypes.py_object)).contents.value

            # convert to a string and pass to the python callback
            data_bytes = ctypes.string_at(data_ptr, length)
            data_str = data_bytes.decode('utf-8')
            self._py_callback(data_str)
        except Exception as e:
            if self is not None and self.exception is None:
                self.exception = e  # keep the first only as they are likely all the same

    def __init__(self, py_callback: Callable[[str], None]):
        self._py_callback = py_callback
        self.exception = None


class CoreInterop:
    """ctypes FFI layer for the Foundry Local Core native library.

    Provides ``execute_command`` and ``execute_command_with_callback`` to
    invoke native commands exposed by ``Microsoft.AI.Foundry.Local.Core``.
    """

    _initialized = False
    _flcore_library = None
    _genai_library = None
    _ort_library = None

    instance = None

    # Callback function for native interop.
    # This returns a string and its length, and an optional user provided object.
    CALLBACK_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)

    @staticmethod
    def _initialize_native_libraries() -> Path:
        """Load the native Foundry Local Core library and its dependencies.

        Locates the binaries from the installed Python packages
        ``foundry-local-core``, ``onnxruntime-core``, and
        ``onnxruntime-genai-core`` using :func:`get_native_binary_paths`.

        Returns:
            Path to the directory that contains the Core binary.
        """
        paths = get_native_binary_paths()
        if paths is None:
            raise RuntimeError(
                "Could not locate native libraries.\n"
                "  Standard variant : pip install foundry-local-sdk\n"
                "  WinML variant    : pip install foundry-local-sdk-winml\n"
                "  Dev/CI install   : foundry-local-install  (or --winml)"
            )

        logger.info("Native libraries found — Core: %s  ORT: %s  GenAI: %s",
                    paths.core, paths.ort, paths.genai)

        if sys.platform.startswith("win"):
            # Register every binary directory so the .NET AOT Core library
            # can resolve sibling DLLs via P/Invoke.
            for native_dir in paths.all_dirs():
                os.add_dll_directory(str(native_dir))
        else:
            # On macOS/Linux, add all binary directories to the library
            # search path so that filepaths can be resolved
            env_var = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
            extra_dirs = os.pathsep.join(str(d) for d in paths.all_dirs())
            existing = os.environ.get(env_var, "")
            os.environ[env_var] = f"{extra_dirs}{os.pathsep}{existing}" if existing else extra_dirs

        # Explicitly pre-load ORT and GenAI so their symbols are globally
        # available when Core does P/Invoke lookups at runtime.
        # On Windows the PATH manipulation above is sufficient; on
        # Linux/macOS we need RTLD_GLOBAL so that dlopen() within the
        # Core native code can resolve ORT/GenAI symbols.
        # ORT must be loaded before GenAI (GenAI depends on ORT).
        if sys.platform.startswith("win"):
            CoreInterop._ort_library = ctypes.CDLL(str(paths.ort))
            CoreInterop._genai_library = ctypes.CDLL(str(paths.genai))
        else:
            CoreInterop._ort_library = ctypes.CDLL(str(paths.ort), mode=os.RTLD_GLOBAL)
            CoreInterop._genai_library = ctypes.CDLL(str(paths.genai), mode=os.RTLD_GLOBAL)

        CoreInterop._flcore_library = ctypes.CDLL(str(paths.core))

        # Set the function signatures
        lib = CoreInterop._flcore_library
        lib.execute_command.argtypes = [ctypes.POINTER(RequestBuffer),
                                        ctypes.POINTER(ResponseBuffer)]
        lib.execute_command.restype = None

        lib.free_response.argtypes = [ctypes.POINTER(ResponseBuffer)]
        lib.free_response.restype = None

        # Set the callback function signature and delegate info
        lib.execute_command_with_callback.argtypes = [ctypes.POINTER(RequestBuffer),
                                                      ctypes.POINTER(ResponseBuffer),
                                                      ctypes.c_void_p,  # callback_fn
                                                      ctypes.c_void_p]  # user_data
        lib.execute_command_with_callback.restype = None

        return paths.core_dir

    @staticmethod
    def _to_c_buffer(s: str):
        # Helper: encodes strings into unmanaged memory
        if s is None:
            return ctypes.c_void_p(0), 0, None
        
        buf = s.encode("utf-8")
        ptr = ctypes.create_string_buffer(buf)  # keeps memory alive in Python
        return ctypes.cast(ptr, ctypes.c_void_p), len(buf), ptr

    def __init__(self, config: Configuration):
        if not CoreInterop._initialized:
            native_dir = CoreInterop._initialize_native_libraries()
            CoreInterop._initialized = True

            # Pass the full path to the Core DLL so the native layer can
            # discover sibling DLLs via Path.GetDirectoryName(FoundryLocalCorePath).
            flcore_lib_name = f"Microsoft.AI.Foundry.Local.Core{_get_ext()}"
            config.foundry_local_core_path = str(native_dir / flcore_lib_name)

            # Auto-detect WinML Bootstrap: if the Bootstrap DLL is present
            # in the native binaries directory and the user hasn't explicitly
            # set the Bootstrap config, enable it automatically.
            if sys.platform.startswith("win"):
                bootstrap_dll = native_dir / "Microsoft.WindowsAppRuntime.Bootstrap.dll"
                if bootstrap_dll.exists():
                    if config.additional_settings is None:
                        config.additional_settings = {}
                    if "Bootstrap" not in config.additional_settings:
                        logger.info("WinML Bootstrap DLL detected — enabling Bootstrap")
                        config.additional_settings["Bootstrap"] = "true"

        request = InteropRequest(params=config.as_dictionary())
        response = self.execute_command("initialize", request)
        if response.error is not None:
            raise FoundryLocalException(f"Failed to initialize Foundry.Local.Core: {response.error}")

        logger.info("Foundry.Local.Core initialized successfully: %s", response.data)

    def _execute_command(self, command: str, interop_request: InteropRequest = None,
                         callback: CoreInterop.CALLBACK_TYPE = None):
        cmd_ptr, cmd_len, cmd_buf = CoreInterop._to_c_buffer(command)
        data_ptr, data_len, data_buf = CoreInterop._to_c_buffer(interop_request.to_json() if interop_request else None)

        req = RequestBuffer(Command=cmd_ptr, CommandLength=cmd_len, Data=data_ptr, DataLength=data_len)
        resp = ResponseBuffer()
        lib = CoreInterop._flcore_library

        if (callback is not None):
            # If a callback is provided, use the execute_command_with_callback method
            # We need a helper to do the initial conversion from ctypes to Python and pass it through to the
            # provided callback function
            callback_helper = CallbackHelper(callback)
            callback_py_obj = ctypes.py_object(callback_helper)
            callback_helper_ptr = ctypes.cast(ctypes.pointer(callback_py_obj), ctypes.c_void_p)
            callback_fn = CoreInterop.CALLBACK_TYPE(CallbackHelper.callback)

            lib.execute_command_with_callback(ctypes.byref(req), ctypes.byref(resp), callback_fn, callback_helper_ptr)

            if callback_helper.exception is not None:
                raise callback_helper.exception
        else:
            lib.execute_command(ctypes.byref(req), ctypes.byref(resp))

        req = None  # Free Python reference to request

        response_str = ctypes.string_at(resp.Data, resp.DataLength).decode("utf-8") if resp.Data else None
        error_str = ctypes.string_at(resp.Error, resp.ErrorLength).decode("utf-8") if resp.Error else None

        # C# owns the memory in the response so we need to free it explicitly
        lib.free_response(resp)
        
        return Response(data=response_str, error=error_str)

    def execute_command(self, command_name: str, command_input: Optional[InteropRequest] = None) -> Response:
        """Execute a command synchronously.

        Args:
            command_name: The native command name (e.g. ``"get_model_list"``).
            command_input: Optional request parameters.

        Returns:
            A ``Response`` with ``data`` on success or ``error`` on failure.
        """
        logger.debug("Executing command: %s Input: %s", command_name,
                     command_input.params if command_input else None)

        response = self._execute_command(command_name, command_input)
        return response

    def execute_command_with_callback(self, command_name: str, command_input: Optional[InteropRequest],
                                      callback: Callable[[str], None]) -> Response:
        """Execute a command with a streaming callback.

        The ``callback`` receives incremental string data from the native layer
        (e.g. streaming chat tokens or download progress).

        Args:
            command_name: The native command name.
            command_input: Optional request parameters.
            callback: Called with each incremental string response.

        Returns:
            A ``Response`` with ``data`` on success or ``error`` on failure.
        """
        logger.debug("Executing command with callback: %s Input: %s", command_name,
                     command_input.params if command_input else None)
        response = self._execute_command(command_name, command_input, callback)
        return response


def get_cached_model_ids(core_interop: CoreInterop) -> list[str]:
    """Get the list of models that have been downloaded and are cached."""

    response = core_interop.execute_command("get_cached_models")
    if response.error is not None:
        raise FoundryLocalException(f"Failed to get cached models: {response.error}")

    try:
        model_ids = json.loads(response.data)
    except json.JSONDecodeError as e:
        raise FoundryLocalException(f"Failed to decode JSON response: Response was: {response.data}") from e

    return model_ids

