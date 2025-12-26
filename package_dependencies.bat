@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title API Detector - 打包依赖

echo.
echo ================================================================
echo              API Detector 依赖打包工具
echo ================================================================
echo.

cd /d "%~dp0"

REM Check Qt6
echo [1/3] 检查 Qt6...
qmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Qt6 未找到
    echo [INFO] 请先安装 Qt6: https://www.qt.io/download-qt-installer
    pause
    exit /b 1
)

echo [OK] Qt6 已安装
qmake --version

REM Find Qt6 path
for /f "delims=" %%i in ('where qmake') do set "QMAKE_PATH=%%i"
for %%i in ("%QMAKE_PATH%") do set "QT_BIN_PATH=%%~dpi"
for %%i in ("%QT_BIN_PATH%..") do set "QT_PATH=%%~fi"

echo [INFO] Qt6 路径: %QT_PATH%

REM Step 2: Build GUI and Launcher
echo.
echo [2/3] 构建 GUI 和启动器...
if exist "build-gui" rmdir /s /q "build-gui"
mkdir build-gui
cd build-gui

cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake 配置失败
    cd ..
    pause
    exit /b 1
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] 构建失败
    cd ..
    pause
    exit /b 1
)

cd ..

echo [OK] 构建完成

REM Step 3: Package dependencies
echo.
echo [3/3] 打包依赖...

set "RELEASE_DIR=release"
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

REM Copy executables
if exist "build-gui\Release\api-checker-gui.exe" (
    copy "build-gui\Release\api-checker-gui.exe" "%RELEASE_DIR%\" >nul
    echo [OK] 复制 GUI 可执行文件
)

if exist "build-gui\Release\api-detector-launcher.exe" (
    copy "build-gui\Release\api-detector-launcher.exe" "%RELEASE_DIR%\" >nul
    echo [OK] 复制启动器可执行文件
)

REM Deploy Qt runtime for GUI
echo [DEPLOY] 部署 Qt 运行时 (GUI)...
"%QT_BIN_PATH%windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%RELEASE_DIR%\api-checker-gui.exe"
if %errorlevel% equ 0 (
    echo [OK] Qt 运行时部署完成
) else (
    echo [WARNING] Qt 运行时部署失败
)

REM Deploy Qt runtime for Launcher
echo [DEPLOY] 部署 Qt 运行时 (启动器)...
"%QT_BIN_PATH%windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%RELEASE_DIR%\api-detector-launcher.exe"
if %errorlevel% equ 0 (
    echo [OK] 启动器 Qt 运行时部署完成
) else (
    echo [WARNING] 启动器 Qt 运行时部署失败
)

REM Copy documentation
copy "README.md" "%RELEASE_DIR%\" >nul
copy "LICENSE" "%RELEASE_DIR%\" >nul
copy "example_keys.txt" "%RELEASE_DIR%\" >nul
copy "QUICKSTART.md" "%RELEASE_DIR%\" >nul

REM Create dependencies archive
echo [PACKAGE] 创建依赖包...
set "DEPS_DIR=%RELEASE_DIR%\dependencies"
mkdir "%DEPS_DIR%"

REM Copy Qt DLLs to dependencies folder
for %%f in (%RELEASE_DIR%\*.dll) do (
    copy "%%f" "%DEPS_DIR%\" >nul
)

REM Create archive
powershell Compress-Archive -Path "%DEPS_DIR%\*" -DestinationPath "%RELEASE_DIR%\dependencies.zip"

REM Clean up
echo [CLEAN] 清理临时文件...
rmdir /s /q "%DEPS_DIR%"

echo.
echo ================================================================
echo                    ✅ 依赖打包完成！
echo ================================================================
echo.
echo 📁 发布目录: %RELEASE_DIR%
echo 📦 依赖包: %RELEASE_DIR%\dependencies.zip
echo.
echo 💡 说明:
echo    - 所有 Qt 运行时已打包到 dependencies.zip
echo    - 用户解压后双击 api-detector-launcher.exe 即可启动
echo    - 无需安装任何依赖
echo.
pause
