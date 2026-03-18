# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging
import sys

# Inline StrEnum compat shim to avoid importing detail (which triggers core_interop → configuration circular import)
if sys.version_info >= (3, 11):
    from enum import StrEnum
else:
    from enum import Enum

    class StrEnum(str, Enum):
        def __str__(self) -> str:
            return self.value

# Map the python logging levels to the Foundry Local Core names
class LogLevel(StrEnum):
    VERBOSE = "Verbose"
    DEBUG = "Debug"
    INFORMATION = "Information"
    WARNING = "Warning"
    ERROR = "Error"
    FATAL = "Fatal"

LOG_LEVEL_MAP = {
    LogLevel.VERBOSE: logging.DEBUG,  # No direct equivalent for Trace in Python logging
    LogLevel.DEBUG: logging.DEBUG,
    LogLevel.INFORMATION: logging.INFO,
    LogLevel.WARNING: logging.WARNING,
    LogLevel.ERROR: logging.ERROR,
    LogLevel.FATAL: logging.CRITICAL,
}

def set_default_logger_severity(config_level: LogLevel):
    py_level = LOG_LEVEL_MAP.get(config_level, logging.INFO)
    logger = logging.getLogger(__name__.split(".", maxsplit=1)[0])
    logger.setLevel(py_level)
