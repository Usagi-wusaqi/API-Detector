@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title API Detector - 自动构建和打包

echo.
echo ================================================================
echo              API Detector 自动构建和打包
echo ================================================================
echo.

cd /d "%~dp0"

REM Create temp directory
set "TEMP_DIR=%TEMP%\api-detector-build"
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"

REM Step 1: Check CMake
echo [1/6] 检查 CMake...
cmake --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] CMake 已安装
    cmake --version
    goto :check_compiler
)

echo [INFO] CMake 未找到，正在下载...
set "CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-windows-x86_64.zip"
set "CMAKE_FILE=%TEMP_DIR%\cmake.zip"

echo [DOWNLOAD] 下载 CMake...
powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%CMAKE_URL%' -OutFile '%CMAKE_FILE%' }"

if not exist "%CMAKE_FILE%" (
    echo [ERROR] 下载 CMake 失败
    pause
    exit /b 1
)

echo [EXTRACT] 解压 CMake...
powershell -Command "Expand-Archive -Path '%CMAKE_FILE%' -DestinationPath 'C:\Program Files\CMake' -Force"

echo [PATH] 添加 CMake 到 PATH...
setx PATH "%PATH%;C:\Program Files\CMake\bin" /M >nul 2>&1
set "PATH=%PATH%;C:\Program Files\CMake\bin"

echo [OK] CMake 安装成功
cmake --version

:check_compiler
REM Step 2: Check Compiler
echo.
echo [2/6] 检查编译器...
cl >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Visual Studio (MSVC) 已安装
    goto :check_git
)

g++ --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MinGW-w64 已安装
    g++ --version
    goto :check_git
)

echo [INFO] 编译器未找到，正在安装 MSYS2...
set "MSYS2_URL=https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe"
set "MSYS2_FILE=%TEMP_DIR%\msys2-installer.exe"

echo [DOWNLOAD] 下载 MSYS2 安装程序 (可能需要一些时间)...
powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%MSYS2_URL%' -OutFile '%MSYS2_FILE%' -UseBasicParsing }"

if not exist "%MSYS2_FILE%" (
    echo [ERROR] 下载 MSYS2 失败
    pause
    exit /b 1
)

echo [INSTALL] 安装 MSYS2 (可能需要一些时间)...
start /wait "" "%MSYS2_FILE%" install --root C:\msys64 --default-profile mingw64

echo [PATH] 添加 MSYS2 到 PATH...
setx PATH "%PATH%;C:\msys64\mingw64\bin" /M >nul 2>&1
set "PATH=%PATH%;C:\msys64\mingw64\bin"

echo [OK] MSYS2 安装成功
g++ --version

:check_git
REM Step 3: Check Git
echo.
echo [3/6] 检查 Git...
git --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Git 已安装
    git --version
    goto :check_qt
)

echo [INFO] Git 未找到，正在下载...
set "GIT_URL=https://github.com/git-for-windows/git/releases/download/v2.43.0.windows.1/MinGit-2.43.0-64-bit.zip"
set "GIT_FILE=%TEMP_DIR%\git.zip"

echo [DOWNLOAD] 下载 Git...
powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%GIT_URL%' -OutFile '%GIT_FILE%' }"

if not exist "%GIT_FILE%" (
    echo [ERROR] 下载 Git 失败
    pause
    exit /b 1
)

echo [EXTRACT] 解压 Git...
powershell -Command "Expand-Archive -Path '%GIT_FILE%' -DestinationPath 'C:\Program Files\Git' -Force"

echo [PATH] 添加 Git 到 PATH...
setx PATH "%PATH%;C:\Program Files\Git\cmd" /M >nul 2>&1
set "PATH=%PATH%;C:\Program Files\Git\cmd"

echo [OK] Git 安装成功
git --version

:check_qt
REM Step 4: Check Qt6
echo.
echo [4/6] 检查 Qt6...
qmake --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Qt6 已安装
    qmake --version
    goto :build_gui
)

echo [INFO] Qt6 未找到，正在安装 aqt 工具...

REM Check Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Python 未找到，正在下载...
    set "PYTHON_URL=https://www.python.org/ftp/python/3.11.8/python-3.11.8-amd64.exe"
    set "PYTHON_FILE=%TEMP_DIR%\python-installer.exe"

    echo [DOWNLOAD] 下载 Python...
    powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%PYTHON_URL%' -OutFile '%PYTHON_FILE%' }"

    if not exist "%PYTHON_FILE%" (
        echo [ERROR] 下载 Python 失败
        pause
        exit /b 1
    )

    echo [INSTALL] 安装 Python...
    start /wait "" "%PYTHON_FILE%" /quiet InstallAllUsers=1 PrependPath=1

    echo [OK] Python 安装成功
    python --version
)

echo [INSTALL] 安装 aqt 工具...
pip install aqtinstall

echo [DOWNLOAD] 下载 Qt6 (这将需要一些时间，约1GB)...
aqt install-qt windows desktop 6.5.0 win64_msvc2019_64 -O "C:\Qt"

echo [PATH] 添加 Qt6 到 PATH...
setx PATH "%PATH%;C:\Qt\6.5.0\msvc2019_64\bin" /M >nul 2>&1
set "PATH=%PATH%;C:\Qt\6.5.0\msvc2019_64\bin"

echo [OK] Qt6 安装成功
qmake --version

:build_gui
REM Step 5: Build GUI
echo.
echo [5/6] 构建 GUI 版本...
echo.

if exist "build-gui" rmdir /s /q "build-gui"
mkdir build-gui
cd build-gui

echo [CONFIG] 运行 CMake 配置...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake 配置失败
    cd ..
    pause
    exit /b 1
)

echo [BUILD] 编译项目 (可能需要一些时间)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] 构建失败
    cd ..
    pause
    exit /b 1
)

cd ..

echo [OK] GUI 和 Launcher 构建完成
echo.

REM Step 6: Package
echo [6/6] 打包发布版本...
echo.

set "RELEASE_DIR=release"
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

REM Copy GUI executable
if exist "build-gui\Release\api-checker-gui.exe" (
    set "GUI_EXEC=build-gui\Release\api-checker-gui.exe"
) else if exist "build-gui\api-checker-gui.exe" (
    set "GUI_EXEC=build-gui\api-checker-gui.exe"
)

copy "%GUI_EXEC%" "%RELEASE_DIR%\api-checker-gui.exe" >nul

REM Copy Launcher executable
if exist "build-gui\Release\api-detector-launcher.exe" (
    set "LAUNCHER_EXEC=build-gui\Release\api-detector-launcher.exe"
) else if exist "build-gui\api-detector-launcher.exe" (
    set "LAUNCHER_EXEC=build-gui\api-detector-launcher.exe"
)

copy "%LAUNCHER_EXEC%" "%RELEASE_DIR%\api-detector-launcher.exe" >nul

REM Deploy Qt runtime for GUI
echo [DEPLOY] 部署 Qt 运行时 (GUI)...
"C:\Qt\6.5.0\msvc2019_64\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%RELEASE_DIR%\api-checker-gui.exe"

REM Deploy Qt runtime for Launcher
echo [DEPLOY] 部署 Qt 运行时 (Launcher)...
"C:\Qt\6.5.0\msvc2019_64\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%RELEASE_DIR%\api-detector-launcher.exe"

REM Package dependencies for offline installation
echo [DEPLOY] 打包依赖...
set "DEPS_DIR=%RELEASE_DIR%\dependencies"
mkdir "%DEPS_DIR%"

REM Copy Qt DLLs to dependencies folder
echo [COPY] 复制 Qt DLLs...
for %%f in (%RELEASE_DIR%\*.dll) do (
    copy "%%f" "%DEPS_DIR%\" >nul
)

REM Create dependencies archive
echo [PACKAGE] 创建依赖包...
powershell Compress-Archive -Path "%DEPS_DIR%\*" -DestinationPath "%RELEASE_DIR%\dependencies.zip"

REM Remove temporary dependencies folder
rmdir /s /q "%DEPS_DIR%"

REM Copy other files
copy "GUI启动器.bat" "%RELEASE_DIR%\" >nul
copy "README.md" "%RELEASE_DIR%\" >nul
copy "LICENSE" "%RELEASE_DIR%\" >nul
copy "example_keys.txt" "%RELEASE_DIR%\" >nul
copy "QUICKSTART.md" "%RELEASE_DIR%\" >nul

REM Create package
echo [PACKAGE] 创建发布包...
powershell Compress-Archive -Path "%RELEASE_DIR%\*" -DestinationPath "api-detector-gui-windows.zip"

REM Clean up
echo.
echo [CLEAN] 清理临时文件...
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"

echo.
echo ================================================================
echo                    ✅ 构建和打包完成！
echo ================================================================
echo.
echo 📁 发布目录: %RELEASE_DIR%
echo 📦 发布包: api-detector-gui-windows.zip
echo.
echo 💡 提示:
echo    - 可以将 api-detector-gui-windows.zip 发布到 GitHub
echo    - 用户解压后双击 api-detector-launcher.exe 即可启动
echo    - 首次启动时会自动安装依赖，无需联网
echo    - 后续启动将直接进入程序界面
echo.
pause
