# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging


def get_logger():
    """
    Get the logger for the sdk.

    Returns:
        logging.Logger: Logger instance for the module.
    """
    return logging.getLogger(__name__.split(".", maxsplit=1)[0])


def set_verbosity(verbose):
    """
    Set the verbosity level for the logger.

    Args:
        verbose (int): Verbosity level (e.g., logging.INFO, logging.DEBUG).
    """
    get_logger().setLevel(verbose)


def set_verbosity_info():
    """Set the verbosity level to INFO."""
    set_verbosity(logging.INFO)


def set_verbosity_warning():
    """Set the verbosity level to WARNING."""
    set_verbosity(logging.WARNING)


def set_verbosity_debug():
    """Set the verbosity level to DEBUG."""
    set_verbosity(logging.DEBUG)


def set_verbosity_error():
    """Set the verbosity level to ERROR."""
    set_verbosity(logging.ERROR)


def set_verbosity_critical():
    """Set the verbosity level to CRITICAL."""
    set_verbosity(logging.CRITICAL)


def get_verbosity() -> int:
    """
    Get the current verbosity level of the logger.

    Returns:
        int: Verbosity level as an integer.
    """
    return get_logger().getEffectiveLevel()


def get_logger_level(level):
    """
    Get Python logging level for the integer level.

    Args:
        level (int): Verbosity level (0: DEBUG, 1: INFO, 2: WARNING, 3: ERROR, 4: CRITICAL).

    Returns:
        int: Corresponding Python logging level.

    Raises:
        ValueError: If the level is invalid.
    """
    level_map = {0: logging.DEBUG, 1: logging.INFO, 2: logging.WARNING, 3: logging.ERROR, 4: logging.CRITICAL}
    # check if level is valid
    if level not in level_map:
        raise ValueError(f"Invalid level {level}, should be one of {list(level_map.keys())}")

    return level_map[level]


def set_default_logger_severity(level):
    """
    Set the default log level for the logger.

    Args:
        level (int): Verbosity level (0: DEBUG, 1: INFO, 2: WARNING, 3: ERROR, 4: CRITICAL).
    """
    # set logger level
    set_verbosity(get_logger_level(level))
