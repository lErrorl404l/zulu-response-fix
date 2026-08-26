@echo off
setlocal enabledelayedexpansion
title Zulu Response Fix - Uninstaller

echo ============================================
echo   Zulu Response Fix - Uninstaller
echo ============================================
echo.
echo This removes the fix. The game returns to its original state.
echo.

:: Find the Steam installation via the registry.
set "STEAM="
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM=%%b"
if not defined STEAM for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM=%%b"
if not defined STEAM (
    echo Steam was not found. Delete the file dinput8.dll from the game's
    echo Binaries\Win32 folder manually.
    pause
    exit /b 1
)

:: Find the game on any Steam library drive.
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
    echo Could not find the game folder. Nothing to remove.
    pause
    exit /b 1
)

if exist "%TARGET%\dinput8.dll" (
    del /q "%TARGET%\dinput8.dll"
    echo Removed the fix from: %TARGET%
) else (
    echo The fix is not installed. Nothing to remove.
)
echo.
pause