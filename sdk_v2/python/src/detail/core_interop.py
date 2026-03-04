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
from .native_downloader import get_native_path, download_native_binaries

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
        return json.dumps({"Params": self.params}, ensure_ascii=False)


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
    ``error`` is ``None`` when the command succeeded.
    """

    data: Optional[str] = None
    error: Optional[str] = None


class CallbackHelper:
    """Internal helper class to convert the callback from ctypes to a str and call the python callback."""
    @staticmethod
    def callback(data_ptr, length, self_ptr):
        try:
            self = ctypes.cast(self_ptr, ctypes.POINTER(ctypes.py_object)).contents.value

            # convert to a string and pass to the python callback
            data_bytes = ctypes.string_at(data_ptr, length)
            data_str = data_bytes.decode('utf-8')
            self._py_callback(data_str)
        except Exception as e:
            if self.exception is None:
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

    instance = None

    # Callback function for native interop.
    # This returns a string and its length, and an optional user provided object.
    CALLBACK_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)

    @staticmethod
    def _add_library_extension(name: str) -> str:
        if sys.platform.startswith("win"):
            return f"{name}.dll"
        elif sys.platform.startswith("linux"):
            return f"{name}.so"
        elif sys.platform.startswith("darwin"):
            return f"{name}.dylib"
        else:
            raise NotImplementedError("Unsupported platform")

    @staticmethod
    def _initialize_native_libraries(base_path: str = None) -> Path:
        """Load the native Foundry Local Core library and its dependencies.

        Resolution order:
        1. If ``base_path`` is provided explicitly, use that directory.
        2. Check the ``packages/{platform-key}/`` directory for previously
           downloaded binaries (via ``get_native_path``).
        3. Download the native NuGet packages on the fly (lazy install)
           and then load from the downloaded location.

        Returns:
            Path to the directory containing the native libraries.
        """
        if base_path:
            resolved = Path(base_path).expanduser().resolve()
        else:
            # Check for pre-downloaded binaries
            resolved = get_native_path()
            if resolved is None:
                # Lazy download on first use
                logger.info("Native libraries not found — downloading from NuGet...")
                resolved = download_native_binaries()
                logger.info("Native libraries installed at %s", resolved)

        flcore_lib_name = CoreInterop._add_library_extension("Microsoft.AI.Foundry.Local.Core")
        flcore_dll_path = resolved / flcore_lib_name
        if not flcore_dll_path.exists():
            raise FileNotFoundError(f"Could not find the Foundry Local Core library at {flcore_dll_path}")

        if sys.platform.startswith("win"):
            # Add the native directory to PATH so that P/Invoke within the .NET
            # AOT Core library can find sibling DLLs (e.g. Bootstrap DLL for WinML).
            native_dir_str = str(resolved)
            current_path = os.environ.get("PATH", "")
            if native_dir_str not in current_path:
                os.environ["PATH"] = f"{native_dir_str};{current_path}"

            # we need to explicitly load the ORT and GenAI libraries first to ensure its dependencies load correctly
            ort_lib_name = CoreInterop._add_library_extension("onnxruntime")
            genai_lib_name = CoreInterop._add_library_extension("onnxruntime-genai")
            CoreInterop._ort_library = ctypes.CDLL(str(resolved / ort_lib_name))
            CoreInterop._genai_library = ctypes.CDLL(str(resolved / genai_lib_name))

        CoreInterop._flcore_library = ctypes.CDLL(str(flcore_dll_path))
        
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

        return resolved

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
            lib_path = config.foundry_local_core_path
            native_dir = CoreInterop._initialize_native_libraries(lib_path)
            CoreInterop._initialized = True

            # Pass the full path to the Core DLL so the native layer can
            # discover sibling DLLs (e.g. the WinML Bootstrap DLL) via
            # Path.GetDirectoryName(FoundryLocalCorePath).
            if not config.foundry_local_core_path:
                flcore_lib_name = CoreInterop._add_library_extension("Microsoft.AI.Foundry.Local.Core")
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



