@echo off
setlocal EnableDelayedExpansion
:: ========================================
::  build.bat - Occult Build Script for Windows
:: ========================================
::
:: CMake picks the generator. It already chooses the newest Visual Studio it
:: knows about and finds installed, which is the whole of what the detection
:: this script used to do was for -- and that detection got it wrong: it read
:: catalog_productDisplayVersion, which on an Insiders build is a string like
:: "Insiders [12009...]" with no major version to parse, then fell back to
:: asking for "Visual Studio 17 2022" on a machine that only had 18. CMake
:: then reported "could not find any instance of Visual Studio", which reads
:: like VS is missing rather than like the script asked for the wrong one.
::
:: To pick one yourself:  set OCCULT_GENERATOR=Visual Studio 18 2026

set "BUILD_DIR=build"

:: ANSI colours. The ESC character has to be captured rather than typed, and
:: VirtualTerminalLevel has to be 1 rather than merely present.
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

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo !RED!Error: CMake not found in PATH.!NC!
    echo Install CMake and add it to your PATH.
    exit /b 1
)

:: Only to say what was found. Nothing is parsed out of it. The backtick call
:: stays at the top level: the parser matches parentheses across a whole
:: if/else block before running it, so the (x86) in the path breaks it inside
:: one even when quoted.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vswhere
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if defined VS_PATH echo Visual Studio: !VS_PATH!
goto :configure

:no_vswhere
echo !YELLOW!Note: vswhere not found, so this cannot say which Visual Studio!NC!
echo !YELLOW!CMake will use. Carrying on; CMake will say if there is none.!NC!

:configure
echo.
if defined OCCULT_GENERATOR goto :configure_named

echo Configuring with clang-cl, CMake choosing the generator...
cmake -S . -B "%BUILD_DIR%" -A x64 -T ClangCL -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF
if %errorlevel% equ 0 goto :build
goto :configure_fresh

:configure_named
echo Configuring with clang-cl, generator "%OCCULT_GENERATOR%"...
cmake -S . -B "%BUILD_DIR%" -G "%OCCULT_GENERATOR%" -A x64 -T ClangCL -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF
if %errorlevel% equ 0 goto :build
goto :configure_fresh

:configure_fresh
:: A cache left by an earlier run with a different generator or platform fails
:: with "does not match the platform used previously", which is not clang-cl's
:: fault. Clear it and try clang-cl once more before blaming the toolset.
if not exist "%BUILD_DIR%\CMakeCache.txt" goto :configure_msvc
echo.
echo !YELLOW!Stale CMake cache; clearing it and retrying clang-cl.!NC!
echo.
rmdir /s /q "%BUILD_DIR%" 2>nul
if defined OCCULT_GENERATOR goto :configure_fresh_named
cmake -S . -B "%BUILD_DIR%" -A x64 -T ClangCL -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF
if %errorlevel% equ 0 goto :build
goto :configure_msvc

:configure_fresh_named
cmake -S . -B "%BUILD_DIR%" -G "%OCCULT_GENERATOR%" -A x64 -T ClangCL -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF
if %errorlevel% equ 0 goto :build
goto :configure_msvc

:configure_msvc
echo.
echo !YELLOW!clang-cl did not configure. Retrying with MSVC.!NC!
echo !YELLOW!Install "C++ Clang tools for Windows" in the VS Installer for clang-cl.!NC!
echo.
rmdir /s /q "%BUILD_DIR%" 2>nul
cmake -S . -B "%BUILD_DIR%" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF
if %errorlevel% equ 0 goto :build

echo.
echo !RED!CMake configuration failed.!NC!
echo.
echo Things to check:
echo   1. "Desktop development with C++" is installed in the VS Installer
echo   2. CMake is new enough for your Visual Studio ^(cmake --version^)
echo   3. Delete the %BUILD_DIR% folder and try again
echo   4. Name a generator yourself: set OCCULT_GENERATOR=Visual Studio 18 2026
echo      ^(cmake --help lists the ones this CMake has^)
exit /b 1

:build
echo.
echo ========================================
echo  Building (Release)...
echo ========================================
echo.
cmake --build "%BUILD_DIR%" --config Release --parallel
if %errorlevel% neq 0 (
    echo.
    echo !RED!Build failed.!NC!
    exit /b 1
)

echo.
echo !GREEN!Build succeeded.!NC!
if exist "%BUILD_DIR%\Release\occultc.exe" echo occultc.exe: %BUILD_DIR%\Release\occultc.exe
if exist "%BUILD_DIR%\occultc.exe" echo occultc.exe: %BUILD_DIR%\occultc.exe
echo.
echo Next: run validate_tests.bat
endlocal
