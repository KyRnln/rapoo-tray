@echo off
setlocal
cd /d "%~dp0"

if not exist bin mkdir bin

taskkill /F /IM RapooTray.exe >nul 2>nul
taskkill /F /IM rapoo-tray.exe >nul 2>nul

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Programs\w64devkit\bin\g++.exe" (
        set "PATH=%LOCALAPPDATA%\Programs\w64devkit\bin;%PATH%"
    )
)

where dotnet >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Microsoft\dotnet" (
        set "PATH=%LOCALAPPDATA%\Microsoft\dotnet;%PATH%"
    )
)

echo ========================================================
echo   Building Rapoo Tray (WinUI 3 + Native C++ HID Core)
echo ========================================================

echo [1/3] Compiling Native C++ Hardware Telemetry Core (rapoo_core.dll) ...
g++ -shared -o bin\rapoo_core.dll src\rapoo_core.cpp src\device_manager.cpp src\rapoo_protocol.cpp src\taskbar_bar.cpp -O2 -std=c++17 -lhid -lsetupapi -lgdiplus -ldwmapi -lcomctl32 -luser32 -lgdi32 -ladvapi32 -static-libgcc -static-libstdc++
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile rapoo_core.dll
    exit /b 1
)

echo [2/3] Compiling WinUI 3 Application (RapooTray.exe) ...
cd RapooTray
dotnet build -c Release -r win-x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to build WinUI 3 project.
    cd ..
    exit /b 1
)
cd ..

echo [3/3] Syncing portable release files to bin\Release\ ...
if not exist bin\Release mkdir bin\Release
xcopy /E /Y /Q RapooTray\bin\x64\Release\net8.0-windows10.0.19041.0\win-x64\* bin\Release\ >nul

echo.
echo ========================================================
echo   [SUCCESS] Build completed! (Portable / No install needed)
echo   Executable: bin\Release\RapooTray.exe
echo ========================================================
