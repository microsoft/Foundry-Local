# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging

from .detail.utils import StrEnum

# Map the python logging levels to the Foundry Local Core names
class LogLevel(StrEnum):
    Verbose = "Trace"
    Debug = "Debug"
    Information = "Information"
    Warning = "Warning"
    Error = "Error"
    Fatal = "Critical"

LOG_LEVEL_MAP = {
    LogLevel.Verbose: logging.DEBUG,  # No direct equivalent for Trace in Python logging
    LogLevel.Debug: logging.DEBUG,
    LogLevel.Information: logging.INFO,
    LogLevel.Warning: logging.WARNING,
    LogLevel.Error: logging.ERROR,
    LogLevel.Fatal: logging.CRITICAL,
}

def set_default_logger_severity(config_level: LogLevel):
    py_level = LOG_LEVEL_MAP.get(config_level, logging.INFO)
    logger = logging.getLogger(__name__.split(".", maxsplit=1)[0])
    logger.setLevel(py_level)
