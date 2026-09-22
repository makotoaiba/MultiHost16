@echo off
setlocal
title MultiHost 16 - Install

rem ------------------------------------------------------------
rem  MultiHost 16 installer for testers.
rem  No administrator rights required.
rem  Put this .bat next to the "MultiHost 16.vst3" folder and run it.
rem  See TESTERS.md for Japanese instructions.
rem ------------------------------------------------------------

set "SRC=%~dp0MultiHost 16.vst3"
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"

if not exist "%SRC%" (
    echo [ERROR] Folder "MultiHost 16.vst3" was not found.
    echo         Put this .bat in the same folder as "MultiHost 16.vst3".
    pause
    exit /b 1
)

echo Installing to: %DEST%
echo.

if not exist "%DEST%" mkdir "%DEST%"

if exist "%DEST%\MultiHost 16.vst3" (
    echo Removing previous version...
    rmdir /s /q "%DEST%\MultiHost 16.vst3"
)

xcopy "%SRC%" "%DEST%\MultiHost 16.vst3" /E /I /Y /Q
if errorlevel 1 goto error

rem ---------- install the out-of-process scanner ----------
set "MH_APPDATA=%APPDATA%\MultiHost16"

if exist "%~dp0MultiHost16Scanner.exe" (
    if not exist "%MH_APPDATA%" mkdir "%MH_APPDATA%"
    copy /Y "%~dp0MultiHost16Scanner.exe" "%MH_APPDATA%\" >nul
    if errorlevel 1 (
        echo [WARNING] Could not copy MultiHost16Scanner.exe
    ) else (
        echo Scanner installed: %MH_APPDATA%\MultiHost16Scanner.exe
    )
) else (
    echo [WARNING] MultiHost16Scanner.exe was not found next to this .bat.
    echo           Plugin scanning will be disabled.
)

echo.
echo ============================================================
echo  INSTALL OK
echo.
echo  1. Start your DAW
echo  2. Cubase: Studio ^> VST Plug-in Manager ^> Rescan
echo  3. Make sure this path is in the VST3 search paths:
echo       %DEST%
echo ============================================================
pause
exit /b 0

:error
echo.
echo *** COPY FAILED - close your DAW and run this again ***
pause
exit /b 1
