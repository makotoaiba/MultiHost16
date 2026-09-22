@echo off
setlocal
title MultiHost 16 - Build

rem ------------------------------------------------------------
rem  MultiHost 16 build script (Windows / Visual Studio 2022 or 2026)
rem  See README.md for Japanese instructions.
rem
rem  This script downloads JUCE as a ZIP (no Git required).
rem ------------------------------------------------------------

set "JUCE_VERSION=8.0.6"
set "JUCE_ZIP=juce_download.zip"

echo ============================================================
echo  MultiHost 16 - build
echo ============================================================
echo.

rem Work inside the folder that contains this .bat, so that every
rem path below can stay relative. This avoids the trailing
rem backslash of %~dp0 breaking quoted arguments.
pushd "%~dp0"

rem ---------- check cmake ----------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake was not found in PATH.
    echo         Install CMake from https://cmake.org/download/
    echo         and enable "Add CMake to the system PATH".
    goto fail
)

echo === CMake version ===
cmake --version
echo.

rem ---------- get JUCE ----------
if exist "JUCE\modules\juce_core\juce_core.h" goto have_juce

echo === JUCE %JUCE_VERSION% not found locally ===
echo.

where tar >nul 2>&1
if errorlevel 1 goto no_tools

if exist "%JUCE_ZIP%" (
    echo Reusing already downloaded %JUCE_ZIP%
    echo.
    goto extract
)

where curl >nul 2>&1
if errorlevel 1 goto no_tools

echo === Downloading (about 25 MB, only once) ===
curl -L --fail -o "%JUCE_ZIP%" "https://github.com/juce-framework/JUCE/archive/refs/tags/%JUCE_VERSION%.zip"
if errorlevel 1 goto dl_fail
echo.

:extract
echo === Extracting ===
if exist "JUCE-%JUCE_VERSION%" rmdir /s /q "JUCE-%JUCE_VERSION%"

tar -xf "%JUCE_ZIP%"
if errorlevel 1 goto dl_fail

if not exist "JUCE-%JUCE_VERSION%\modules" goto dl_fail

if exist "JUCE" rmdir /s /q "JUCE"
move "JUCE-%JUCE_VERSION%" "JUCE" >nul
if errorlevel 1 goto dl_fail

if not exist "JUCE\modules\juce_core\juce_core.h" goto dl_fail

del "%JUCE_ZIP%" >nul 2>&1
echo JUCE ready.
echo.

:have_juce

set "JUCE_DIR=%CD%\JUCE"

rem ---------- drop a stale cache from an earlier failed run ----------
if exist "build\CMakeCache.txt" (
    findstr /C:"JUCE_PATH:" "build\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 (
        echo Removing stale CMake cache from a previous attempt...
        rmdir /s /q "build"
        echo.
    )
)

rem ---------- configure ----------
echo === Configuring ===
cmake -B build -A x64 -DJUCE_PATH="%JUCE_DIR%"
if errorlevel 1 goto fail

rem ---------- build ----------
echo.
echo === Building (Release) ===
cmake --build build --config Release
if errorlevel 1 goto fail

rem ---------- install the out-of-process scanner ----------
rem The scanner runs plugin scanning in a separate process, so a
rem badly behaved plugin cannot take the DAW down with it.
set "MH_APPDATA=%APPDATA%\MultiHost16"
set "MH_SCANNER=build\MultiHost16Scanner_artefacts\Release\MultiHost16Scanner.exe"

if not exist "%MH_SCANNER%" (
    echo [WARNING] Scanner executable was not built.
    echo           Plugin scanning will be disabled in the plugin UI.
    goto skip_scanner
)

if not exist "%MH_APPDATA%" mkdir "%MH_APPDATA%"
copy /Y "%MH_SCANNER%" "%MH_APPDATA%\" >nul
if errorlevel 1 (
    echo [WARNING] Could not copy the scanner to %MH_APPDATA%
) else (
    echo Scanner installed: %MH_APPDATA%\MultiHost16Scanner.exe
)

:skip_scanner

echo.
echo ============================================================
echo  BUILD OK
echo.
echo  Output    : build\MultiHost16_artefacts\Release\VST3\
echo  Installed : %LOCALAPPDATA%\Programs\Common\VST3
echo.
echo  NOTE: close Cubase before building, otherwise the
echo        auto-copy step will fail with a locked file.
echo ============================================================
popd
pause
exit /b 0

:no_tools
echo [ERROR] curl / tar were not found.
echo         They ship with Windows 10 1803 and later.
echo.
goto manual

:dl_fail
echo.
echo [ERROR] Failed to download or extract JUCE.
echo.
goto manual

:manual
echo ------------------------------------------------------------
echo  MANUAL SETUP
echo.
echo  1. Open this URL in a browser:
echo       https://github.com/juce-framework/JUCE/releases/tag/%JUCE_VERSION%
echo  2. Download "Source code (zip)"
echo  3. Extract it
echo  4. Rename the extracted folder "JUCE-%JUCE_VERSION%" to "JUCE"
echo  5. Put that "JUCE" folder next to this .bat file, so that
echo       JUCE\modules\ exists
echo  6. Run this .bat again
echo ------------------------------------------------------------
goto fail

:fail
echo.
echo *** BUILD FAILED - please check the log above ***
popd
pause
exit /b 1
