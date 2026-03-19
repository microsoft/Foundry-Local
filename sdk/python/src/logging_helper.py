# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging

from ._str_enum import StrEnum

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
