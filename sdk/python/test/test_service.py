# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from unittest import mock

import pytest
from foundry_local.service import assert_foundry_installed, get_service_uri, start_service


def test_assert_foundry_installed():
    """Test checking if foundry is installed."""
    # Test when foundry is installed
    with mock.patch("shutil.which") as mock_which:
        mock_which.return_value = "/usr/local/bin/foundry"
        assert_foundry_installed()  # Should not raise an exception

    # Test when foundry is not installed
    with mock.patch("shutil.which") as mock_which:
        mock_which.return_value = None
        with pytest.raises(RuntimeError, match="Foundry is not installed or not on PATH!"):
            assert_foundry_installed()


def test_get_service_uri():
    """Test getting service URI."""
    # Test when service is running
    with mock.patch("subprocess.Popen") as mock_popen:
        mock_process = mock.MagicMock()
        mock_process.communicate.return_value = (
            b"Model management service is running on http://localhost:5273/openai/status",
            b"",
        )
        mock_popen.return_value.__enter__.return_value = mock_process
        assert get_service_uri() == "http://localhost:5273"

    # Test when service is not running
    with mock.patch("subprocess.Popen") as mock_popen:
        mock_process = mock.MagicMock()
        mock_process.communicate.return_value = (b"Model management service is not running!", b"")
        mock_popen.return_value.__enter__.return_value = mock_process
        assert get_service_uri() is None

    # Test with IPv4 address
    with mock.patch("subprocess.Popen") as mock_popen:
        mock_process = mock.MagicMock()
        mock_process.communicate.return_value = (
            b"Model management service is running on http://127.0.0.1:5273/openai/status",
            b"",
        )
        mock_popen.return_value.__enter__.return_value = mock_process
        assert get_service_uri() == "http://127.0.0.1:5273"


def test_start_service():
    """Test starting the service."""
    # Test when service is already running
    with mock.patch("foundry_local.service.get_service_uri") as mock_get_uri:
        mock_get_uri.return_value = "http://localhost:5273"
        assert start_service() == "http://localhost:5273"
        mock_get_uri.assert_called_once()

    # Test when service needs to be started and starts successfully
    with (
        mock.patch("foundry_local.service.get_service_uri") as mock_get_uri,
        mock.patch("subprocess.Popen") as mock_popen,
        mock.patch("time.sleep") as mock_sleep,
    ):
        # First call returns None (not running), second call returns URI (started)
        mock_get_uri.side_effect = [None, "http://localhost:5273"]
        assert start_service() == "http://localhost:5273"
        assert mock_get_uri.call_count == 2
        mock_popen.assert_called_once_with(["foundry", "service", "start"])
        mock_sleep.assert_not_called()

    # Test when service fails to start
    with (
        mock.patch("foundry_local.service.get_service_uri") as mock_get_uri,
        mock.patch("subprocess.Popen") as mock_popen,
        mock.patch("foundry_local.service.time.sleep") as mock_sleep,
    ):
        # Always return None (never starts)
        num_sleeps = 10
        mock_get_uri.return_value = None
        assert start_service() is None
        assert mock_get_uri.call_count == num_sleeps + 1
        mock_popen.assert_called_once_with(["foundry", "service", "start"])
        assert mock_sleep.call_count == num_sleeps
