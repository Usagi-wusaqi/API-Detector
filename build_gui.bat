@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

if "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
) else (
    set "NO_PAUSE=0"
)

echo.
echo ================================================================
echo              Building API Checker GUI v1.0.0
echo ================================================================
echo.

echo [INFO] Checking build environment...

cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found
    echo [INFO] Please install CMake: winget install Kitware.CMake
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)
echo [OK] CMake: Installed

cl >nul 2>&1
if %errorlevel% neq 0 (
    g++ --version >nul 2>&1
    if %errorlevel% neq 0 (
        echo [ERROR] C++ compiler not found
        echo [INFO] Please install one of the following:
        echo    - Visual Studio 2022: winget install Microsoft.VisualStudio.2022.BuildTools
        echo    - MinGW-w64: Download from https://www.mingw-w64.org/
        if %NO_PAUSE% equ 0 pause
        exit /b 1
    ) else (
        echo [OK] Compiler: MinGW-w64
        set COMPILER=MinGW
    )
) else (
    echo [OK] Compiler: Visual Studio (MSVC)
    set COMPILER=MSVC
)

qmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Qt6 not found
    echo [INFO] Please install Qt6:
    echo    - Download: https://www.qt.io/download-qt-installer
    echo    - Or use aqt tool: pip install aqtinstall
    echo    - Install command: aqt install-qt windows desktop 6.5.0 win64_msvc2019_64
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)
echo [OK] Qt6: Installed

echo.
echo [INFO] Starting GUI build...
echo.

if not exist "build-gui" mkdir build-gui
cd build-gui

echo [INFO] Configuring project...
if "%COMPILER%"=="MinGW" (
    cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
) else (
    cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
)

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    echo.
    echo [INFO] Possible solutions:
    echo    1. Make sure Qt6 is installed and added to PATH
    echo    2. Set Qt6_DIR environment variable to Qt6's lib/cmake directory
    echo    3. Check Qt6 version is 6.5.0 or higher
    cd ..
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)

echo [INFO] Building project...
if "%COMPILER%"=="MinGW" (
    mingw32-make -j%NUMBER_OF_PROCESSORS%
) else (
    cmake --build . --config Release -j%NUMBER_OF_PROCESSORS%
)

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    cd ..
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)

cd ..

if exist "build-gui\Release\api-checker-gui.exe" (
    set GUI_EXEC=build-gui\Release\api-checker-gui.exe
) else if exist "build-gui\api-checker-gui.exe" (
    set GUI_EXEC=build-gui\api-checker-gui.exe
) else (
    echo [ERROR] GUI executable not found
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)

if exist "build-gui\Release\api-detector-launcher.exe" (
    set LAUNCHER_EXEC=build-gui\Release\api-detector-launcher.exe
) else if exist "build-gui\api-detector-launcher.exe" (
    set LAUNCHER_EXEC=build-gui\api-detector-launcher.exe
) else (
    echo [ERROR] Launcher executable not found
    if %NO_PAUSE% equ 0 pause
    exit /b 1
)

echo.
echo [OK] Build successful!
echo [INFO] GUI executable location: %GUI_EXEC%
echo [INFO] Launcher executable location: %LAUNCHER_EXEC%

copy "%GUI_EXEC%" "api-checker-gui.exe" >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Copied GUI to root directory: api-checker-gui.exe
)

copy "%LAUNCHER_EXEC%" "api-detector-launcher.exe" >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Copied Launcher to root directory: api-detector-launcher.exe
)

echo.
echo [OK] API Detector GUI build completed!
echo [INFO] Double-click api-detector-launcher.exe to launch the application
echo [INFO] Or double-click api-checker-gui.exe to launch directly
echo.

if %NO_PAUSE% equ 0 pause
exit /b 0
