@echo off
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "REPO=%%~fI"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%REPO%\tools\Invoke-RendererIsolation.ps1" -RepositoryRoot "%REPO%" %*
set "RC=%ERRORLEVEL%"
exit /b %RC%
