@echo off
setlocal
cd /d "%~dp0"

if not exist ..\dist mkdir ..\dist

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Programs\w64devkit\bin\g++.exe" (
        set "PATH=%LOCALAPPDATA%\Programs\w64devkit\bin;%PATH%"
    )
)

echo [Building Installer Resource...]
windres --codepage 65001 -i installer.rc -o installer.o -O coff

echo [Compiling rapoo-tray-setup.exe...]
g++ -O3 -mwindows -municode -s -static installer.cpp installer.o -lole32 -lshell32 -luser32 -lgdi32 -ladvapi32 -luuid -o ..\dist\rapoo-tray-setup.exe

if exist installer.o del installer.o

if exist ..\dist\rapoo-tray-setup.exe (
    echo.
    echo [SUCCESS] Installer built: dist\rapoo-tray-setup.exe
    dir ..\dist\rapoo-tray-setup.exe | findstr rapoo-tray-setup.exe
) else (
    echo.
    echo [ERROR] Failed to build installer!
    exit /b 1
)
