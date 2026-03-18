# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging
import sys

from foundry_local.api import FoundryLocalManager
from foundry_local.version import __version__

_logger = logging.getLogger(__name__)
_logger.setLevel(logging.WARNING)

_sc = logging.StreamHandler(stream=sys.stdout)
_formatter = logging.Formatter(
    "[foundry-local] | %(asctime)s | %(levelname)-8s | %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
)
_sc.setFormatter(_formatter)
_logger.addHandler(_sc)
_logger.propagate = False

__all__ = ["FoundryLocalManager", "__version__"]
