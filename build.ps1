param(
    [string]$OutputDir = "dist\windows-amd64",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$localGo = Join-Path (Split-Path -Parent $repoRoot) ".tools\go\go\bin\go.exe"
$goExe = $null

if (Get-Command go -ErrorAction SilentlyContinue) {
    $goExe = "go"
} elseif (Test-Path $localGo) {
    $goExe = $localGo
} else {
    throw "Go toolchain not found. Install Go or place a local toolchain at $localGo"
}

$outputDirPath = Join-Path $repoRoot $OutputDir
$cacheDir = Join-Path $repoRoot ".cache\go-build"
$modCacheDir = Join-Path $repoRoot ".cache\gomod"

if ($Clean -and (Test-Path $outputDirPath)) {
    Remove-Item -Recurse -Force -LiteralPath $outputDirPath
}

New-Item -ItemType Directory -Force -Path $outputDirPath | Out-Null
New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
New-Item -ItemType Directory -Force -Path $modCacheDir | Out-Null

$env:GOCACHE = $cacheDir
$env:GOMODCACHE = $modCacheDir

Write-Host "Using Go:" $goExe
& $goExe version

$cliPath = Join-Path $outputDirPath "apidetect.exe"
$guiPath = Join-Path $outputDirPath "apidetect-gui.exe"

& $goExe build -trimpath -o $cliPath .\cmd\apidetect
& $goExe build -trimpath -ldflags "-H=windowsgui" -o $guiPath .\cmd\apidetect-gui

Write-Host ""
Write-Host "Build completed:"
Write-Host "  $cliPath"
Write-Host "  $guiPath"
