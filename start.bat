@echo off
setlocal
cd /d "%~dp0"

set "RELEASE=%~dp0bin\x64\Release\Layrix.exe"
set "DEBUG=%~dp0bin\x64\Debug\Layrix.exe"

if exist "%RELEASE%" (
    start "" /D "%~dp0bin\x64\Release" "%RELEASE%"
    exit /b 0
)

if exist "%DEBUG%" (
    start "" /D "%~dp0bin\x64\Debug" "%DEBUG%"
    exit /b 0
)

echo Dev-сборка не найдена. Сначала соберите проект:
echo   powershell -ExecutionPolicy Bypass -File scripts\build.ps1
exit /b 1
