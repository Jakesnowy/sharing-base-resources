<#
.SYNOPSIS
    IntegratedStorage (UE4SS C++ mod) 一键安装环境 + 编译 + 打包脚本（国内网络友好）。

.DESCRIPTION
    本 mod 不能单独编译 —— 它依赖 UE4SS 的 C++ SDK + PolyHook2（RE-UE4SS 自带），并使用
    `ue4ss.mod` 这个 xmake 规则（见 src/xmake.lua）。所以必须在 RE-UE4SS 源码树里编译。

    需要的环境（脚本会自动检测/安装，除 MSVC 外基本全自动）：
      - Git for Windows        (你已装；脚本只检测)
      - xmake                  (脚本可经 winget 或 GitHub 代理自动装)
      - MSVC (VS 2022 Build Tools + "使用 C++ 的桌面开发")  (走微软 CDN，国内可直连；脚本可经 winget 静默装，需你点 UAC)
      - RE-UE4SS 源码          (脚本经 GitHub 代理自动 git clone，含子模块)
    不需要 Node.js / CMake / Python。

    两种用法：
      1) 首次/全新机器：加 -Bootstrap，脚本自动装环境后接着编译打包。
      2) 以后每次：直接运行（环境已就绪），只做 复制源码→编译→组装→zip。

.PARAMETER UE4SSRoot
    RE-UE4SS 本地路径。-Bootstrap 模式下不存在会自动克隆到这里；否则指向你已有目录。
    也可用环境变量 $env:UE4SS_ROOT。

.PARAMETER Bootstrap
    先自动准备环境（装 xmake + 克隆 RE-UE4SS + 检查/装 MSVC），再编译。

.PARAMETER GitHubProxy
    访问 github 用的代理前缀（国内必需）。默认 https://gh-proxy.com/
    设为 "" 禁用代理。备选: https://ghfast.top/  https://ghproxy.net/

.PARAMETER Configuration
    release(默认) 或 debug。

.PARAMETER NoBuild
    与 -Bootstrap 配合：只准备环境，不编译。

.PARAMETER Zip
    额外生成 ModIntegratedStorageCpp.zip（放在项目根，可提交到 git 作为发布产物）。

.PARAMETER Clean
    编译前先 xmake clean。

.EXAMPLE
    # 全新机器，一条命令从零到 mod（MSVC 那步会提示确认）：
    .\build.ps1 -Bootstrap -UE4SSRoot D:\src\RE-UE4SS -Zip
    # 之后每次：
    .\build.ps1 -UE4SSRoot D:\src\RE-UE4SS -Zip
#>
[CmdletBinding()]
param(
    [string]$UE4SSRoot     = $env:UE4SS_ROOT,
    [string]$Configuration = "release",
    [switch]$Bootstrap,
    [string]$GitHubProxy   = "https://gh-proxy.com/",
    [switch]$NoBuild,
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
#  小工具
# ----------------------------------------------------------------------------
function Write-Step($m) { Write-Host ""; Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok($m)   { Write-Host "  [OK]   $m" -ForegroundColor Green }
function Write-Warn($m) { Write-Host "  [WARN] $m" -ForegroundColor Yellow }
function Die($m)        { Write-Host ""; Write-Host "[ERROR] $m" -ForegroundColor Red; exit 1 }
function Test-Cmd($n)   { return [bool](Get-Command $n -ErrorAction SilentlyContinue) }
function Set-Tls12      { try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocol]::Tls12 } catch {} }
function Gh-Url($u) {
    if ($GitHubProxy -and $u -like "https://github.com/*") { return $GitHubProxy.TrimEnd('/') + '/' + $u }
    return $u
}

# ----------------------------------------------------------------------------
#  xmake
# ----------------------------------------------------------------------------
function Get-XmakeExe {
    $c = Get-Command xmake -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $cands = @(
        (Join-Path $env:ProgramFiles "xmake\xmake.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "xmake\xmake.exe"),
        (Join-Path $env:LOCALAPPDATA "xmake\xmake.exe")
    )
    foreach ($p in $cands) { if (Test-Path -LiteralPath $p) { return $p } }
    return $null
}
function Ensure-Xmake {
    $xm = Get-XmakeExe
    if ($xm) { Write-Ok "xmake 已就绪: $xm"; return $xm }
    Write-Step "安装 xmake ..."
    if (Test-Cmd winget) {
        Write-Host "  通过 winget 安装..."
        winget install --id xmake-io.xmake -e --accept-source-agreements --accept-package-agreements 2>&1 | Out-Host
        $xm = Get-XmakeExe
        if ($xm) { Write-Ok "winget 安装 xmake 成功"; return $xm }
        Write-Warn "winget 未生效，改用直接下载安装器..."
    }
    Set-Tls12
    try {
        $api   = Gh-Url "https://api.github.com/repos/xmake-io/xmake/releases/latest"
        $rel   = Invoke-RestMethod -Uri $api -UseBasicParsing
        $asset = $rel.assets | Where-Object { $_.name -like "*win32.exe" -and $_.name -notlike "*curl*" } | Select-Object -First 1
        if (-not $asset) { Die "未找到 xmake win32 安装器。请手动安装: https://xmake.io" }
        $tmp = [IO.Path]::GetTempFileName() + ".exe"
        Write-Host "  下载安装器: $($asset.name) ..."
        Invoke-WebRequest -Uri (Gh-Url $asset.browser_download_url) -OutFile $tmp -UseBasicParsing
        Write-Host "  静默安装 (/S)..."
        Start-Process -FilePath $tmp -ArgumentList "/S" -Wait
        Remove-Item $tmp -Force -ErrorAction SilentlyContinue
    } catch {
        Die "xmake 自动安装失败: $($_.Exception.Message)`n请手动安装 xmake: https://xmake.io  (装完重开终端再运行)"
    }
    $xm = Get-XmakeExe
    if (-not $xm) { Die "xmake 安装后仍找不到。请重开终端再运行，或手动加入 PATH。" }
    Write-Ok "xmake 安装成功: $xm"
    return $xm
}

# ----------------------------------------------------------------------------
#  MSVC (via vswhere)
# ----------------------------------------------------------------------------
function Get-Vswhere {
    $p = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $p) { return $p }
    return $null
}
function Test-MSVC {
    $v = Get-Vswhere
    if (-not $v) { return $false }
    $out = & $v -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    return [bool]$out
}
function Ensure-MSVC {
    if (Test-MSVC) { Write-Ok "MSVC (VC++ 工具集) 已安装"; return }
    Write-Step "安装 MSVC (Visual Studio 2022 Build Tools + 使用 C++ 的桌面开发)"
    Write-Warn "约数 GB 下载，来自微软 CDN（国内可直连），耗时较长。"
    Write-Host "  若不想脚本自动装，可手动下载页: https://visualstudio.microsoft.com/visual-cpp-build-tools/" -ForegroundColor Gray
    $ans = Read-Host "  现在让脚本经 winget 自动安装？(Y/N，默认 Y)"
    if ($ans -and $ans.Trim() -notmatch '^[Yy]') {
        Start-Process "https://visualstudio.microsoft.com/visual-cpp-build-tools/"
        Die "已取消。请手动安装 VS 2022 Build Tools 并勾选『使用 C++ 的桌面开发』后重运行。"
    }
    if (Test-Cmd winget) {
        Write-Host "  winget 静默安装 Build Tools + VC++ 工作负载（请允许 UAC 提权）..."
        winget install Microsoft.VisualStudio.2022.BuildTools `
            --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --includeRecommended" `
            --accept-source-agreements --accept-package-agreements 2>&1 | Out-Host
        if (Test-MSVC) { Write-Ok "MSVC 安装完成"; return }
        Write-Warn "winget 安装后仍未检测到 VC++，可能需重启终端/机器。"
    }
    Start-Process "https://visualstudio.microsoft.com/visual-cpp-build-tools/"
    Die "自动安装未完成，已打开官方下载页。请手动安装并勾选『使用 C++ 的桌面开发』，装完重运行。"
}

# ----------------------------------------------------------------------------
#  RE-UE4SS 源码（经 GitHub 代理克隆，含子模块；-c 仅作用于本次命令，不污染全局配置）
# ----------------------------------------------------------------------------
function Ensure-UE4SS {
    if ($UE4SSRoot -and (Test-Path -LiteralPath $UE4SSRoot)) {
        $n = (Get-ChildItem -LiteralPath $UE4SSRoot -Force -ErrorAction SilentlyContinue | Measure-Object).Count
        if ($n -gt 0) { Write-Ok "RE-UE4SS 已存在: $UE4SSRoot"; return }
    }
    if (-not $UE4SSRoot) { Die "未指定 -UE4SSRoot，且目录不存在，无法克隆 RE-UE4SS。" }
    Write-Step "克隆 RE-UE4SS (含子模块，经 GitHub 代理) ..."
    $proxies = @($GitHubProxy) + @("https://ghfast.top/", "https://ghproxy.net/", "")
    $proxies = $proxies | Select-Object -Unique
    $done = $false
    foreach ($px in $proxies) {
        if ((Test-Path -LiteralPath $UE4SSRoot)) { Remove-Item -LiteralPath $UE4SSRoot -Recurse -Force -ErrorAction SilentlyContinue }
        $cfgArgs = @()
        if ($px) { $cfgArgs = @("-c", "url.$($px.TrimEnd('/'))/https://github.com/.insteadOf=https://github.com/") }
        Write-Host "  尝试克隆 (代理: $(if ($px) { $px } else { '直连' }))..."
        & git @cfgArgs clone --recursive "https://github.com/UE4SS-RE/RE-UE4SS.git" $UE4SSRoot 2>&1 | Out-Host
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath (Join-Path $UE4SSRoot "xmake.lua"))) { $done = $true; break }
        Write-Warn "此代理失败，换下一个..."
    }
    if (-not $done) { Die "克隆 RE-UE4SS 失败（所有代理都不通）。请自备可用 GitHub 代理/VPN 完成一次克隆，或用 -GitHubProxy 指定一个。" }
    Write-Ok "克隆完成: $UE4SSRoot"
}

# ----------------------------------------------------------------------------
#  Bootstrap：准备环境
# ----------------------------------------------------------------------------
$XmakeExe = $null
if ($Bootstrap) {
    Write-Step "[Bootstrap] 准备编译环境"
    if (-not (Test-Cmd git)) {
        Start-Process "https://git-scm.com/download/win"
        Die "未检测到 git。请先安装 Git for Windows 后重运行。"
    }
    Write-Ok "git 已就绪"
    $XmakeExe = Ensure-Xmake
    Ensure-UE4SS
    Ensure-MSVC
    if ($NoBuild) { Write-Ok "环境准备完成（-NoBuild，已跳过编译）"; exit 0 }
} else {
    if (-not $UE4SSRoot -or -not (Test-Path -LiteralPath $UE4SSRoot)) {
        Die "缺少 -UE4SSRoot 或目录不存在。首次使用请加 -Bootstrap 让脚本自动准备环境。"
    }
    $XmakeExe = Get-XmakeExe
    if (-not $XmakeExe) { Die "未检测到 xmake。首次使用请加 -Bootstrap 自动安装。" }
}

# ----------------------------------------------------------------------------
#  源码自检
# ----------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $SrcXmake)) { Die "找不到 src\xmake.lua: $SrcXmake" }
if (-not (Test-Path -LiteralPath $SrcCpp))   { Die "找不到 src\dllmain.cpp: $SrcCpp" }

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
        & $XmakeExe clean; if ($LASTEXITCODE -ne 0) { Die "xmake clean 失败 (exit $LASTEXITCODE)" }
    }
    Write-Step "xmake 配置 ($Configuration) ..."
    & $XmakeExe f -m $Configuration -y
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "若卡在拉取依赖(polyhook 等)，多为 github 网络问题；本 mod 的 polyhook 来自 RE-UE4SS 自带 deps，正常无需联网。"
        Die "xmake 配置失败 (exit $LASTEXITCODE)。确认 MSVC 已装(VS Build Tools + 使用 C++ 的桌面开发)。"
    }
    Write-Step "xmake 编译 $ModName ..."
    & $XmakeExe build $ModName
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  若提示 'target not found'，请在 RE-UE4SS 根 xmake.lua 里加一行:" -ForegroundColor Yellow
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
    $found = Get-ChildItem -Path $buildDir -Recurse -Filter "$ModName.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $built = $found.FullName }
}
if (-not $built) { Die "编译成功但在 $buildDir 下找不到 $ModName.dll，请手动搜索。" }
Write-Ok "找到 $built"

# ----------------------------------------------------------------------------
#  4) 组装可安装 mod
# ----------------------------------------------------------------------------
Write-Step "组装可安装 mod..."
New-Item -ItemType Directory -Force -Path (Split-Path $OutDll) | Out-Null
Copy-Item $built $OutDll -Force
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
Write-Host " 可安装 mod 文件夹: $DistMod" -ForegroundColor White
if ($Zip) { Write-Host " 发布用 zip: $ProjectRoot\$ModName.zip  (此 zip 未被 .gitignore 忽略，可直接 git add 提交)" -ForegroundColor White }
Write-Host " 安装: 把 $ModName 文件夹复制到游戏 UE4SS\Mods\" -ForegroundColor Cyan
Write-Host "   服务器: <PalServer>\Pal\Saved\Plugins\UE4SS\Mods\" -ForegroundColor Gray
Write-Host "   客户端: <游戏>\Pal\Binaries\Win64\UE4SS\Mods\" -ForegroundColor Gray
Write-Host " 进游戏 UE4SS.log 出现 '[ISGATE] === IntegratedStorage ... loaded ===' 即成功" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
