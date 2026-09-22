@echo off
setlocal
title MultiHost 16 - Environment check

rem ------------------------------------------------------------
rem  Reports what exists and what is missing.
rem  Run this from the project folder and send the output
rem  if something is not working.
rem ------------------------------------------------------------

pushd "%~dp0"

echo ============================================================
echo  MultiHost 16 - environment check
echo ============================================================
echo.
echo Project folder : %CD%
echo.

echo --- tools ---
where cmake >nul 2>&1 && (cmake --version | findstr /R "version") || echo cmake            : NOT FOUND in PATH
where curl  >nul 2>&1 && echo curl             : found || echo curl             : NOT FOUND
where tar   >nul 2>&1 && echo tar              : found || echo tar              : NOT FOUND
where powershell >nul 2>&1 && echo powershell       : found || echo powershell       : NOT FOUND
echo.

echo --- scripts in this folder ---
call :chkfile "build_windows.bat"
call :chkfile "make_package.bat"
call :chkfile "install_for_test.bat"
call :chkfile "CMakeLists.txt"
call :chkdir  "Source"
call :chkdir  "Scanner"
echo.

echo --- JUCE ---
call :chkdir  "JUCE"
call :chkfile "JUCE\modules\juce_core\juce_core.h"
echo.

echo --- build output ---
call :chkdir  "build"
call :chkdir  "build\MultiHost16_artefacts\Release\VST3\MultiHost 16.vst3"
call :chkfile "build\MultiHost16Scanner_artefacts\Release\MultiHost16Scanner.exe"
echo.

echo --- package output ---
call :chkdir  "dist"
echo.

echo --- installed locations ---
call :chkdir  "%LOCALAPPDATA%\Programs\Common\VST3\MultiHost 16.vst3"
call :chkfile "%APPDATA%\MultiHost16\MultiHost16Scanner.exe"
call :chkfile "%APPDATA%\MultiHost16\MultiHost16.settings"
echo.

echo --- old version leftovers (should be gone) ---
call :chkdir  "%LOCALAPPDATA%\Programs\Common\VST3\MultiHost 4.vst3"
call :chkdir  "%APPDATA%\MultiHost4"
echo.

echo ============================================================
echo  What to do next
echo ============================================================
if not exist "make_package.bat" (
    echo  make_package.bat is missing.
    echo  -^> Extract the latest MultiHost16_src.zip over this folder.
    goto done
)
if not exist "build\MultiHost16_artefacts\Release\VST3\MultiHost 16.vst3" (
    echo  The Release plugin has not been built yet.
    echo  -^> Run build_windows.bat and wait for "BUILD OK".
    goto done
)
if not exist "dist" (
    echo  Build looks fine, but the package has not been made.
    echo  -^> Run make_package.bat
    goto done
)
echo  Everything looks present. dist\ is ready to send.

:done
echo ============================================================
popd
pause
exit /b 0

:chkfile
if exist %1 (echo   [OK]      %~1) else (echo   [MISSING] %~1)
exit /b 0

:chkdir
if exist %1\ (echo   [OK]      %~1\) else (echo   [MISSING] %~1\)
exit /b 0
