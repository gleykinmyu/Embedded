@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update-compile-commands.ps1" %*
exit /b %errorlevel%
