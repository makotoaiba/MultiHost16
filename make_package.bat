@echo off
setlocal
title MultiHost 16 - Make distribution package

rem ------------------------------------------------------------
rem  Collects everything a tester needs into dist\ and zips it.
rem  Run this AFTER build_windows.bat has succeeded.
rem  A log is written to make_package_log.txt
rem ------------------------------------------------------------

set "PKG_VERSION=1.6.0"
set "PKG_NAME=MultiHost16_v%PKG_VERSION%"

pushd "%~dp0"
set "LOG=%CD%\make_package_log.txt"
echo MultiHost 16 packaging log > "%LOG%"
echo folder: %CD% >> "%LOG%"

echo ============================================================
echo  MultiHost 16 - packaging %PKG_NAME%
echo ============================================================
echo  folder : %CD%
echo  log    : make_package_log.txt
echo.

set "VST3_SRC=build\MultiHost16_artefacts\Release\VST3\MultiHost 16.vst3"
set "SCANNER_SRC=build\MultiHost16Scanner_artefacts\Release\MultiHost16Scanner.exe"
set "DEST=dist\%PKG_NAME%"

echo [1/6] checking build output
if not exist "%VST3_SRC%\" (
    echo       [ERROR] not found: %VST3_SRC%
    echo ERROR: vst3 missing >> "%LOG%"
    echo       Run build_windows.bat first and wait for "BUILD OK".
    goto fail
)
echo       ok: plugin bundle
if not exist "%SCANNER_SRC%" (
    echo       [ERROR] not found: %SCANNER_SRC%
    echo ERROR: scanner missing >> "%LOG%"
    goto fail
)
echo       ok: scanner

echo [2/6] preparing dist folder
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%" >> "%LOG%" 2>&1
if not exist "%DEST%\" (
    echo       [ERROR] could not create %DEST%
    echo       The folder may be read-only, or blocked by antivirus.
    goto fail
)
echo       ok: %DEST%

echo [3/6] copying plugin bundle
xcopy "%VST3_SRC%" "%DEST%\MultiHost 16.vst3" /E /I /Y /Q >> "%LOG%" 2>&1
if errorlevel 1 (
    echo       [ERROR] xcopy failed - see make_package_log.txt
    goto fail
)
echo       ok

echo [4/6] copying scanner
copy /Y "%SCANNER_SRC%" "%DEST%\" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo       [ERROR] copy failed - see make_package_log.txt
    goto fail
)
echo       ok

echo [5/6] copying installer and docs
copy /Y "install_for_test.bat" "%DEST%\" >> "%LOG%" 2>&1
copy /Y "TESTERS.md"           "%DEST%\" >> "%LOG%" 2>&1
copy /Y "MANUAL.md"            "%DEST%\" >> "%LOG%" 2>&1
echo       ok

echo [6/6] creating zip
set "ZIP=dist\%PKG_NAME%.zip"
if exist "%ZIP%" del /q "%ZIP%"

rem try tar first (ships with Windows 10 1803+, handles zip)
tar -a -c -f "%ZIP%" -C "dist" "%PKG_NAME%" >> "%LOG%" 2>&1

if not exist "%ZIP%" (
    echo       tar did not work, trying PowerShell
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path 'dist\%PKG_NAME%' -DestinationPath 'dist\%PKG_NAME%.zip' -Force" >> "%LOG%" 2>&1
)

if exist "%ZIP%" (
    echo       ok: %ZIP%
) else (
    echo       [WARNING] zip could not be created.
    echo                 The folder %DEST% is still complete -
    echo                 you can right-click it and use "Send to ^> Compressed folder".
)

echo.
echo ============================================================
echo  PACKAGE READY
echo.
echo   Folder : %CD%\%DEST%
if exist "%ZIP%" echo   Zip    : %CD%\%ZIP%
echo.
echo  Send these to your testers:
echo    MultiHost 16.vst3\        (the plugin bundle - a FOLDER)
echo    MultiHost16Scanner.exe    (required for plugin scanning)
echo    install_for_test.bat
echo    TESTERS.md / MANUAL.md
echo.
echo  Before sending, check:
echo    [ ] Built in RELEASE (not Debug)
echo    [ ] Tested on another PC if possible
echo    [ ] Tell testers to right-click the zip, Properties, Unblock
echo    [ ] Licence terms reviewed (JUCE / VST3 SDK) - see README.md
echo ============================================================
echo.
echo Opening the dist folder...
start "" "%CD%\dist"
popd
pause
exit /b 0

:fail
echo.
echo *** PACKAGING FAILED ***
echo Please send the contents of make_package_log.txt
echo ============================================================
popd
pause
exit /b 1
