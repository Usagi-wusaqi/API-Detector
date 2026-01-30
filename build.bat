@echo off
chcp 65001 >nul
echo ========================================
echo   API Detector - C# 跨平台编译脚本
echo ========================================
echo.

dotnet --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 .NET SDK
    echo 下载: https://dotnet.microsoft.com/download/dotnet/8.0
    pause
    exit /b 1
)

echo [1/4] 清理旧构建...
if exist "bin" rd /s /q "bin"
if exist "obj" rd /s /q "obj"
if exist "ApiDetector.exe" del /f /q "ApiDetector.exe"
if exist "ApiDetector.pdb" del /f /q "ApiDetector.pdb"
if exist "av_libglesv2.dll" del /f /q "av_libglesv2.dll"
if exist "libHarfBuzzSharp.dll" del /f /q "libHarfBuzzSharp.dll"
if exist "libSkiaSharp.dll" del /f /q "libSkiaSharp.dll"

echo [2/4] 还原依赖...
dotnet restore src/ApiDetector.csproj
if errorlevel 1 (
    echo [错误] 还原依赖失败
    pause
    exit /b 1
)

echo [3/4] 编译 Windows x64...
dotnet publish src/ApiDetector.csproj -c Release -r win-x64 -o . --self-contained true
if errorlevel 1 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo [4/4] 完成!
echo.
echo ========================================
echo   输出: ApiDetector.exe
echo ========================================
echo.
echo 其他平台:
echo   macOS:  dotnet publish src/ApiDetector.csproj -c Release -r osx-x64 -o .
echo   Linux:  dotnet publish src/ApiDetector.csproj -c Release -r linux-x64 -o .
echo.
pause
