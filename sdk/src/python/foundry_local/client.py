# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import json
import logging
import re

import httpx
from tqdm import tqdm

logger = logging.getLogger(__name__)


class HttpxClient:
    """
    Client for Foundry Local SDK.

    Attributes:
        _client (httpx.Client): HTTP client instance.
    """

    def __init__(self, host: str, timeout: float | httpx.Timeout | None = None) -> None:
        """
        Initialize the HttpxClient with the host.

        Args:
            host (str): Base URL of the host.
            timeout (float | httpx.Timeout | None): Timeout for the HTTP client.
        """
        self._client = httpx.Client(base_url=host, timeout=timeout)

    def _request(self, *args, **kwargs) -> httpx.Response:
        """
        Send an HTTP request.

        Args:
            *args: Positional arguments for the request.
            **kwargs: Keyword arguments for the request.

        Returns:
            httpx.Response: HTTP response object.

        Raises:
            RuntimeError: If an HTTP error or connection error occurs.
        """
        try:
            response = self._client.request(*args, **kwargs)
            response.raise_for_status()
        except httpx.HTTPStatusError as e:
            raise RuntimeError(f"HTTP error: {e.response.status_code} - {e.response.text}") from None
        except httpx.ConnectError:
            raise RuntimeError(
                "Connection error! Please check if the foundry service is running and the host URL is correct."
            ) from None
        return response

    def get(self, path: str, query_params: dict[str, str] | None = None) -> dict | list | None:
        """
        Send a GET request to the specified path with optional query parameters.

        Args:
            path (str): Path for the GET request.
            query_params (dict[str, str] | None): Query parameters for the request.

        Returns:
            dict | list | None: JSON response or None if no content.
        """
        response = self._request("GET", path, params=query_params)
        return response.json() if response.text else None

    def post_with_progress(self, path: str, body: dict | None = None) -> dict:
        """
        Send a POST request to the specified path with optional request body and show progress.

        Args:
            path (str): Path for the POST request.
            body (dict | None): Request body in JSON format.

        Returns:
            dict: JSON response.

        Raises:
            ValueError: If the JSON response is invalid.
        """
        with self._client.stream("POST", path, json=body, timeout=None) as response:
            progress_bar = None
            prev_percent = 0.0
            if logger.isEnabledFor(logging.INFO):
                progress_bar = tqdm(total=100.0)
            final_json = ""
            for line in response.iter_lines():
                if final_json or line.startswith("{"):
                    final_json += line
                    continue
                if not progress_bar:
                    continue
                if match := re.search(r"(\d+(?:\.\d+)?)%", line):
                    percent = min(float(match.group(1)), 100.0)
                    delta = percent - prev_percent
                    if delta > 0:
                        progress_bar.update(delta)
                        prev_percent = percent
            if progress_bar:
                progress_bar.close()

            if not final_json.endswith("}"):
                raise ValueError(f"Invalid JSON response: {final_json}")

            return json.loads(final_json)
