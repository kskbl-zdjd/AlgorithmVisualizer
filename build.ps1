# AlgoVisualizer 构建脚本
# 使用方法: PowerShell 中运行 .\build.ps1
# 注意: 需要先设置 PATH（见下方），或确保 Qt 已添加到系统 PATH

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ============ 配置区域 ============
$QtRoot     = "E:\QT5\qt5"
$QtVersion  = "6.8.3"
$Toolchain  = "mingw1310_64"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "build"
# ===================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  AlgoVisualizer 构建脚本 v1.1" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 工具链路径
$QtLibBin    = "$QtRoot\bin"
$CompilerBin = "$QtRoot\Tools\$Toolchain\bin"
$CMakeBin    = "$QtRoot\Tools\CMake_64\bin"
$NinjaExe    = "$QtRoot\Tools\Ninja\ninja.exe"

# 设置 PATH（关键！）
$env:PATH = "$QtLibBin;$CompilerBin;$CMakeBin;$env:PATH"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$QtRoot\plugins\platforms"

Write-Host "`n[INFO] Qt 版本: $QtVersion" -ForegroundColor Gray
Write-Host "[INFO] 编译器: MinGW $Toolchain" -ForegroundColor Gray
Write-Host "[INFO] CMake: $(& cmake.exe --version | Select-Object -First 1)" -ForegroundColor Gray

Write-Host "`n[1/5] 清理旧构建目录..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "      已删除: $BuildDir" -ForegroundColor Gray
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

Write-Host "[2/5] 配置 CMake (Ninja)..." -ForegroundColor Yellow
Set-Location $BuildDir
& cmake.exe .. -G "Ninja"
if (-not $?) { throw "CMake 配置失败" }

Write-Host "[3/5] 编译项目..." -ForegroundColor Yellow
& $NinjaExe
if (-not $?) { throw "编译失败" }

Write-Host "[4/5] 复制运行时 DLL..." -ForegroundColor Yellow
$binDir = Join-Path $BuildDir "bin"
$pluginsDir = Join-Path $binDir "platforms"
$stylesDir = Join-Path $binDir "styles"

# 复制 Qt DLL
Copy-Item "$QtLibBin\*.dll" -Destination $binDir -ErrorAction SilentlyContinue

# 复制 MinGW 运行时
Copy-Item "$CompilerBin\*.dll" -Destination $binDir -ErrorAction SilentlyContinue
Copy-Item "$CompilerBin\g++.exe" -Destination $binDir -ErrorAction SilentlyContinue
Copy-Item "$CompilerBin\gcc.exe" -Destination $binDir -ErrorAction SilentlyContinue

# 复制平台插件
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
New-Item -ItemType Directory -Path $stylesDir -Force | Out-Null
Copy-Item "$QtRoot\plugins\platforms\*.dll" -Destination $pluginsDir -ErrorAction SilentlyContinue
Copy-Item "$QtRoot\plugins\styles\*.dll" -Destination $stylesDir -ErrorAction SilentlyContinue

Write-Host "[5/5] 完成！" -ForegroundColor Green
$ExePath = Join-Path $binDir "AlgorithmVisualizer.exe"
if (Test-Path $ExePath) {
    $exeSize = [math]::Round((Get-Item $ExePath).Length / 1KB, 1)
    $totalSize = [math]::Round((Get-ChildItem $binDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "  编译成功！" -ForegroundColor Green
    Write-Host "  可执行文件: $ExePath" -ForegroundColor White
    Write-Host "  可执行文件: $exeSize KB" -ForegroundColor White
    Write-Host "  运行时总大小: $totalSize MB" -ForegroundColor White
    Write-Host "========================================" -ForegroundColor Green
} else {
    throw "未找到编译产物: $ExePath"
}
