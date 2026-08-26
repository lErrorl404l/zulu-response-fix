@echo off
setlocal enabledelayedexpansion
title Zulu Response Fix - Installer

echo ============================================
echo   Zulu Response Fix - Installer
echo ============================================
echo.
echo This fixes the game closing at the menu.
echo It copies one file into the game folder.
echo.

:: Find the Steam installation via the registry.
set "STEAM="
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM=%%b"
if not defined STEAM for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM=%%b"
if not defined STEAM (
    echo Steam was not found in the registry.
    echo Open Steam once, close it, then run this installer again.
    pause
    exit /b 1
)
echo Found Steam at: %STEAM%

:: Find the game. It can be on any Steam library drive.
set "TARGET="
if exist "%STEAM%\steamapps\common\Zulu Response\Binaries\Win32" set "TARGET=%STEAM%\steamapps\common\Zulu Response\Binaries\Win32"

for /f "usebackq tokens=1,2,* delims= " %%a in ("%STEAM%\steamapps\libraryfolders.vdf") do (
    if /i "%%a"=="path" (
        set "LIB=%%c"
        set "LIB=!LIB:"=!"
        if exist "!LIB!\steamapps\common\Zulu Response\Binaries\Win32" set "TARGET=!LIB!\steamapps\common\Zulu Response\Binaries\Win32"
    )
)

if not defined TARGET (
    echo.
    echo Could not find the game folder. Is Zulu Response installed?
    echo If it is installed on another drive, move it to this one in Steam,
    echo or run this installer from the game folder.
    pause
    exit /b 1
)
echo Found the game at: %TARGET%
echo.

copy /y "%~dp0dinput8.dll" "%TARGET%\dinput8.dll" >nul
if errorlevel 1 (
    echo The copy failed. Close the game and try again.
    pause
    exit /b 1
)

echo.
echo Done! Launch Zulu Response and play.
echo.
echo If Windows shows "Unknown publisher", click "More info" then "Run anyway".
echo This is normal for a small community mod without a paid signing certificate.
echo.
pause