@echo off
:: Bootstrap: just launch the PowerShell script from the same directory
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0validate_tests.ps1"
exit /b %errorlevel%