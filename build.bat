@echo off
setlocal
cd /d "%~dp0"

if not exist bin mkdir bin
if not exist bin\Release mkdir bin\Release

taskkill /F /IM RapooTray.exe >nul 2>nul
taskkill /F /IM rapoo-tray.exe >nul 2>nul

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Programs\w64devkit\bin\g++.exe" (
        set "PATH=%LOCALAPPDATA%\Programs\w64devkit\bin;%PATH%"
    )
)

if "%DOTNET_ROOT%"=="" (
    if exist "%LOCALAPPDATA%\Microsoft\dotnet" (
        set "DOTNET_ROOT=%LOCALAPPDATA%\Microsoft\dotnet"
    )
)
if exist "%DOTNET_ROOT%" (
    set "PATH=%DOTNET_ROOT%;%PATH%"
)

echo ========================================================
echo   Building Rapoo Tray (WinUI 3 + Native C++ HID Core)
echo ========================================================

echo [1/2] Compiling Native C++ Hardware Telemetry Core (rapoo_core.dll) ...
g++ -shared -o bin\rapoo_core.dll src\rapoo_core.cpp src\device_manager.cpp src\rapoo_protocol.cpp src\taskbar_bar.cpp -O2 -std=c++17 -lhid -lsetupapi -lgdiplus -ldwmapi -lcomctl32 -luser32 -lgdi32 -ladvapi32 -static-libgcc -static-libstdc++
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile rapoo_core.dll
    exit /b 1
)

echo [2/2] Publishing Self-Contained WinUI 3 App (bin\Release\RapooTray.exe) ...
cd RapooTray
dotnet publish -c Release -r win-x64 --self-contained true -o "..\bin\Release"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to publish WinUI 3 project.
    cd ..
    exit /b 1
)
cd ..

echo.
echo ========================================================
echo   [SUCCESS] Build completed! (100%% Portable / Self-Contained)
echo   Output Executable: bin\Release\RapooTray.exe
echo ========================================================
