<#
.SYNOPSIS
    IntegratedStorage (UE4SS C++ mod) 一键编译 + 打包脚本。

.DESCRIPTION
    本 mod 不能单独编译 —— 它依赖 UE4SS 的 C++ SDK + PolyHook2，并使用 `ue4ss.mod`
    这个 xmake 规则（见 src/xmake.lua）。所以必须在 RE-UE4SS 源码树里编译。
    本脚本自动完成：
      1) 把源码复制进 <RE-UE4SS>/cppmods/ModIntegratedStorageCpp/（src/dllmain.cpp 放进 src/ 子目录）
      2) 用 xmake 编译
      3) 找到产出的 dll，落到 dist/ModIntegratedStorageCpp/dlls/main.dll
      4) （可选）把可安装的 mod 文件夹打成 zip

.PARAMETER UE4SSRoot
    你的 RE-UE4SS 本地克隆路径。必填（或设置环境变量 $env:UE4SS_ROOT）。

.PARAMETER Configuration
    编译配置：release（默认）或 debug。

.PARAMETER Zip
    额外生成 ModIntegratedStorageCpp.zip（放在项目根目录）。

.PARAMETER Clean
    编译前先 xmake clean。

.EXAMPLE
    .\build.ps1 -UE4SSRoot D:\src\RE-UE4SS
    .\build.ps1 -UE4SSRoot D:\src\RE-UE4SS -Zip -Clean
#>
[CmdletBinding()]
param(
    [string]$UE4SSRoot     = $env:UE4SS_ROOT,
    [string]$Configuration = "release",
    [switch]$Zip,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------------------
#  路径常量
# ----------------------------------------------------------------------------
$ProjectRoot = $PSScriptRoot
if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition }

$ModName  = "ModIntegratedStorageCpp"
$SrcXmake = Join-Path $ProjectRoot "src\xmake.lua"
$SrcCpp   = Join-Path $ProjectRoot "src\dllmain.cpp"
$DistMod  = Join-Path $ProjectRoot "dist\$ModName"
$OutDll   = Join-Path $DistMod "dlls\main.dll"

# ----------------------------------------------------------------------------
#  小工具：彩色输出
# ----------------------------------------------------------------------------
function Write-Step([string]$msg) { Write-Host ""
                                  Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg)   { Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Die([string]$msg)        { Write-Host ""
                                  Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

# ----------------------------------------------------------------------------
#  前置检查
# ----------------------------------------------------------------------------
Write-Step "检查环境..."

if (-not $UE4SSRoot) {
    Die "缺少 RE-UE4SS 路径。用法：.\build.ps1 -UE4SSRoot <你的RE-UE4SS路径>  （或设置 `$env:UE4SS_ROOT）"
}
$UE4SSRoot = (Resolve-Path $UE4SSRoot -ErrorAction SilentlyContinue)?.Path
if (-not (Test-Path -LiteralPath $UE4SSRoot)) { Die "UE4SSRoot 路径不存在：$UE4SSRoot" }

if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) {
    Die "找不到 xmake。请先安装：https://xmake.io  （装完重开终端）"
}
if (-not (Get-Command cl -ErrorAction SilentlyContinue) -and -not (Get-Command clang -ErrorAction SilentlyContinue)) {
    Write-Host "  [警告] PATH 里没找到 cl/clang —— 请确保已在「VS 开发者命令提示符」里运行，" -ForegroundColor Yellow
    Write-Host "         或先运行 vcvarsall.bat / 加载 MSVC 环境。" -ForegroundColor Yellow
}

if (-not (Test-Path -LiteralPath $SrcXmake)) { Die "找不到 src\xmake.lua：$SrcXmake" }
if (-not (Test-Path -LiteralPath $SrcCpp))   { Die "找不到 src\dllmain.cpp：$SrcCpp" }
Write-Ok "环境就绪"

# ----------------------------------------------------------------------------
#  1) 复制源码进 UE4SS 树（xmake.lua 在根，dllmain.cpp 进 src/ 子目录）
# ----------------------------------------------------------------------------
Write-Step "复制源码到 UE4SS 树..."
$ModInTree = Join-Path $UE4SSRoot "cppmods\$ModName"
$InTreeSrc = Join-Path $ModInTree "src"
New-Item -ItemType Directory -Force -Path $InTreeSrc | Out-Null
Copy-Item $SrcXmake (Join-Path $ModInTree "xmake.lua") -Force
Copy-Item $SrcCpp   (Join-Path $InTreeSrc "dllmain.cpp") -Force
Write-Ok "已写入 $ModInTree"

# ----------------------------------------------------------------------------
#  2) xmake 编译
# ----------------------------------------------------------------------------
Push-Location $UE4SSRoot
try {
    if ($Clean) {
        Write-Step "xmake clean..."
        xmake clean; if ($LASTEXITCODE -ne 0) { Die "xmake clean 失败 (exit $LASTEXITCODE)" }
    }

    Write-Step "xmake 配置 ($Configuration)（首次会拉依赖，请耐心等待）..."
    xmake f -m $Configuration -y
    if ($LASTEXITCODE -ne 0) { Die "xmake 配置失败 (exit $LASTEXITCODE)。确认 MSVC 环境已加载。" }

    Write-Step "xmake 编译 $ModName ..."
    xmake build $ModName
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  若提示 'target not found'，说明 RE-UE4SS 没自动收录该 cppmod。" -ForegroundColor Yellow
        Write-Host "  请在 RE-UE4SS 根的 xmake.lua 里按现有 cppmod 方式 includes 一下：" -ForegroundColor Yellow
        Write-Host "    includes(`"cppmods/$ModName/xmake.lua`")" -ForegroundColor Yellow
        Die "xmake 编译失败 (exit $LASTEXITCODE)"
    }
} finally { Pop-Location }
Write-Ok "编译完成"

# ----------------------------------------------------------------------------
#  3) 定位产物 dll
# ----------------------------------------------------------------------------
Write-Step "定位编译产物..."
$buildDir = Join-Path $UE4SSRoot "build"
$candidates = @(
    (Join-Path $buildDir "windows\x64\$Configuration\$ModName.dll"),
    (Join-Path $buildDir "windows\$Configuration\$ModName.dll"),
    (Join-Path $buildDir "$Configuration\$ModName.dll")
)
$built = $null
foreach ($c in $candidates) { if (Test-Path -LiteralPath $c) { $built = $c; break } }
if (-not $built -and (Test-Path -LiteralPath $buildDir)) {
    $found = Get-ChildItem -Path $buildDir -Recurse -Filter "$ModName.dll" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if ($found) { $built = $found.FullName }
}
if (-not $built) { Die "编译成功但在 $buildDir 下找不到 $ModName.dll，请手动搜索。" }
Write-Ok "找到 $built"

# ----------------------------------------------------------------------------
#  4) 落到 dist/.../dlls/main.dll
# ----------------------------------------------------------------------------
Write-Step "组装可安装 mod..."
New-Item -ItemType Directory -Force -Path (Split-Path $OutDll) | Out-Null
Copy-Item $built $OutDll -Force

# 确保骨架文件存在
foreach ($f in @("enabled.txt", "config.txt")) {
    $p = Join-Path $DistMod $f
    if (-not (Test-Path -LiteralPath $p)) { New-Item -ItemType File -Path $p -Force | Out-Null }
}
Write-Ok "已生成 $OutDll"

# ----------------------------------------------------------------------------
#  5) （可选）打 zip
# ----------------------------------------------------------------------------
if ($Zip) {
    Write-Step "打包 zip..."
    $zipPath = Join-Path $ProjectRoot "$ModName.zip"
    if (Test-Path -LiteralPath $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $DistMod "*") -DestinationPath $zipPath -Force
    Write-Ok "已生成 $zipPath"
}

# ----------------------------------------------------------------------------
#  完成
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " 打包完成！" -ForegroundColor Cyan
Write-Host " 可安装的 mod 文件夹：" -ForegroundColor Cyan
Write-Host "   $DistMod" -ForegroundColor White
Write-Host " 安装方法：把 $ModName 整个文件夹复制到游戏的 UE4SS Mods 目录：" -ForegroundColor Cyan
Write-Host "   服务器: <PalServer>\Pal\Saved\Plugins\UE4SS\Mods\" -ForegroundColor Gray
Write-Host "   客户端: <游戏>\Pal\Binaries\Win64\UE4SS\Mods\" -ForegroundColor Gray
Write-Host " 进游戏后 UE4SS.log 出现 '[ISGATE] === IntegratedStorage ... loaded ===' 即成功" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
