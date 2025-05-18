# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from unittest import mock

import httpx
import pytest
from foundry_local.client import HttpResponseError, HttpxClient


def test_initialization():
    """Test initialization of HttpxClient."""
    with mock.patch("httpx.Client") as mock_client:
        HttpxClient("http://localhost:5273")
        mock_client.assert_called_once_with(base_url="http://localhost:5273", timeout=None)

        # Test with timeout
        HttpxClient("http://localhost:5273", timeout=30.0)
        mock_client.assert_called_with(base_url="http://localhost:5273", timeout=30.0)


# pylint: disable=protected-access
def test_request():
    """Test _request method."""
    with mock.patch("httpx.Client") as mock_client:
        mock_instance = mock_client.return_value
        client = HttpxClient("http://localhost:5273")

        # Test successful request
        mock_response = mock.MagicMock()
        mock_response.raise_for_status.return_value = None
        mock_instance.request.return_value = mock_response

        result = client._request("GET", "/test")
        mock_instance.request.assert_called_once_with("GET", "/test")
        assert result == mock_response

        # Test HTTP error
        mock_response = mock.MagicMock()
        mock_response.raise_for_status.side_effect = httpx.HTTPStatusError(
            "Error", request=mock.MagicMock(), response=mock.MagicMock()
        )
        mock_response.status_code = 404
        mock_response.text = "Not Found"
        mock_instance.request.return_value = mock_response

        with pytest.raises(HttpResponseError):
            client._request("GET", "/test")

        # Test connection error
        mock_instance.request.side_effect = httpx.ConnectError("Connection failed")

        with pytest.raises(ConnectionError, match="Could not connect to Foundry Local"):
            client._request("GET", "/test")


def test_get():
    """Test get method."""
    with mock.patch.object(HttpxClient, "_request") as mock_request:
        client = HttpxClient("http://localhost:5273")

        # Test with JSON response
        mock_response = mock.MagicMock()
        mock_response.text = '{"key": "value"}'
        mock_response.json.return_value = {"key": "value"}
        mock_request.return_value = mock_response

        result = client.get("/test")
        mock_request.assert_called_once_with("GET", "/test", params=None)
        assert result == {"key": "value"}

        # Test with empty response
        mock_response.text = ""
        result = client.get("/test")
        assert result is None

        # Test with query parameters
        mock_response.text = '{"key": "value"}'
        result = client.get("/test", query_params={"param": "value"})
        mock_request.assert_called_with("GET", "/test", params={"param": "value"})
        assert result == {"key": "value"}


def test_post_with_progress():
    """Test post_with_progress method."""
    with (
        mock.patch("httpx.Client") as mock_client,
        mock.patch("foundry_local.client.tqdm") as mock_tqdm,
        mock.patch("foundry_local.client.logger") as mock_logger,
    ):
        mock_instance = mock_client.return_value
        mock_logger_instance = mock.MagicMock()
        mock_logger_instance.isEnabledFor.return_value = True
        mock_logger.return_value = mock_logger_instance

        # Mock the stream context manager
        mock_response = mock.MagicMock()
        mock_instance.stream.return_value.__enter__.return_value = mock_response

        # Mock tqdm progress bar
        mock_progress_bar = mock.MagicMock()
        mock_tqdm.return_value = mock_progress_bar

        # Mock iter_lines to return progress and JSON
        mock_response.iter_lines.return_value = [
            "Progress: 10.1%",
            "Progress: 50%",
            "Progress: 100%",
            '{"status": "complete"}',
        ]

        client = HttpxClient("http://localhost:5273")
        result = client.post_with_progress("/test", body={"key": "value"})

        mock_instance.stream.assert_called_once_with("POST", "/test", json={"key": "value"}, timeout=None)
        mock_progress_bar.update.assert_any_call(10.1)  # 10.1% - 0%
        mock_progress_bar.update.assert_any_call(39.9)  # 50% - 10.1%
        mock_progress_bar.update.assert_any_call(50.0)  # 100% - 50%
        mock_progress_bar.close.assert_called_once()
        assert result == {"status": "complete"}

        # Test invalid JSON response
        mock_response.iter_lines.return_value = ["Progress: 100%", '{"status": "incomplete']

        with pytest.raises(ValueError, match="Invalid JSON response"):
            client.post_with_progress("/test", body={"key": "value"})

        # Test with no progress bar (logging disabled)
        mock_logger_instance.isEnabledFor.return_value = False
        mock_response.iter_lines.return_value = ['{"status": "complete"}']

        result = client.post_with_progress("/test", body={"key": "value"})
        assert result == {"status": "complete"}
