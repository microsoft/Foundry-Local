from __future__ import annotations


class FoundryLocalException(Exception):
    """Base exception for all Foundry Local SDK errors."""

    def __init__(self, message: str, error_code: int = 0) -> None:
        super().__init__(message)
        self.error_code = error_code
