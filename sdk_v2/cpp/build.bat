:: Copyright (c) Microsoft Corporation. All rights reserved.
:: Licensed under the MIT License.

@echo off
setlocal

rem Requires a Python install to be available in your PATH
python "%~dp0\build.py" %*
