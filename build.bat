@echo off
setlocal EnableDelayedExpansion
:: ========================================
::  build.bat - Occult Build Script for Windows
::  Dynamic Visual Studio 2026 / 2022 detection + clang-cl
:: ========================================

:: Enable ANSI colors
:: BUG FIX 1: Capture the ESC character (0x1B) properly — without it,
::            color variables were just plain text like "[0;31m" with no effect.
:: BUG FIX 2: Check that VirtualTerminalLevel is set to 1 (enabled),
::            not just that the registry key exists.
set "RED="
set "GREEN="
set "YELLOW="
set "NC="
for /f %%a in ('echo prompt $E^| cmd') do set "ESC=%%a"
reg query HKCU\Console /v VirtualTerminalLevel 2>nul | findstr /i "0x00000001" >nul
if %errorlevel% equ 0 (
    set "RED=!ESC![0;31m"
    set "GREEN=!ESC![0;32m"
    set "YELLOW=!ESC![1;33m"
    set "NC=!ESC![0m"
)

echo ========================================
echo  Occult Build Script (Windows + clang-cl)
echo ========================================
echo.

:: ========================================
:: Dynamically detect Visual Studio version using vswhere
:: ========================================
set "GENERATOR="
set "VS_PATH="

:: %VSWHERE% is expanded with regular %% syntax so it works correctly at the
:: top level. for /f backtick calls must NOT be nested inside parenthesized
:: if/else blocks -- the batch parser matches parentheses across the whole block
:: at parse time, so (x86) in the expanded path causes "was unexpected at this
:: time" even inside quotes. Using goto keeps the for /f calls at the top level.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

echo Searching for Visual Studio...
if not exist "%VSWHERE%" goto :vswhere_not_found

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH goto :vswhere_not_found

echo Detected Visual Studio at: !VS_PATH!

for /f "usebackq tokens=*" %%v in (`"%VSWHERE%" -latest -prerelease -property catalog_productDisplayVersion`) do set "VS_VERSION=%%v"
for /f "tokens=1 delims=." %%m in ("!VS_VERSION!") do set "MAJOR_VER=%%m"

if "!MAJOR_VER!"=="18" (
    set "GENERATOR=Visual Studio 18 2026"
    echo Detected: Visual Studio 2026
) else if "!MAJOR_VER!"=="17" (
    set "GENERATOR=Visual Studio 17 2022"
    echo Detected: Visual Studio 2022
) else (
    echo !YELLOW!Warning: Unrecognized VS major version ^(!MAJOR_VER!^). Falling back to VS 2022 generator.!NC!
    set "GENERATOR=Visual Studio 17 2022"
)
goto :vs_detected

:vswhere_not_found
echo !YELLOW!Warning: vswhere did not detect VS. Falling back to VS 2026 generator...!NC!
set "GENERATOR=Visual Studio 18 2026"

:vs_detected

if not defined GENERATOR (
    echo !RED!Error: Could not detect Visual Studio.!NC!
    exit /b 1
)

echo Using CMake Generator: !GENERATOR!
echo.

:: ========================================
:: Build Configuration
:: ========================================
set "BUILD_DIR=build"
set "ARCH=x64"
set "TOOLSET=ClangCL"

:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo !RED!Error: CMake not found in PATH.!NC!
    echo Please install CMake and add it to your PATH.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Configuring CMake with clang-cl toolset...
echo.
cmake -S . -B "%BUILD_DIR%" ^
      -G "!GENERATOR!" ^
      -A %ARCH% ^
      -T %TOOLSET% ^
      -DCMAKE_CXX_STANDARD=23 ^
      -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
      -DCMAKE_CXX_EXTENSIONS=OFF

if %errorlevel% neq 0 (
    echo.
    echo !RED!CMake configuration failed.!NC!
    echo.
    echo Possible fixes:
    echo   1. Make sure "C++ Clang tools for Windows" is installed in Visual Studio Installer
    echo   2. Try running this script from Developer Command Prompt for VS 2026
    echo   3. Delete the build folder and try again
    exit /b 1
)

echo.
echo ========================================
echo  Building with clang-cl (Release)...
echo ========================================
echo.
cmake --build "%BUILD_DIR%" --config Release -- /m

if %errorlevel% equ 0 (
    echo.
    echo !GREEN!Build succeeded!!NC!
    echo occultc.exe should be at: %BUILD_DIR%\Release\occultc.exe
    echo.
    echo Next: Run validate_tests.bat
) else (
    echo.
    echo !RED!Build failed.!NC!
    exit /b 1
)

endlocal