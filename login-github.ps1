$env:HTTPS_PROXY = "http://127.0.0.1:7897"
$env:HTTP_PROXY  = "http://127.0.0.1:7897"
$gh = "C:\Program Files\GitHub CLI\gh.exe"

Write-Host "==> 用浏览器登录 GitHub（设备码流程）" -ForegroundColor Cyan
& $gh auth login --hostname github.com --git-protocol https --web
if ($LASTEXITCODE -ne 0) { Write-Host "登录失败" -ForegroundColor Red; pause; exit 1 }

Write-Host "==> 让 git 使用 gh 的凭据" -ForegroundColor Cyan
& $gh auth setup-git --hostname github.com

Write-Host "==> 当前登录状态:" -ForegroundColor Cyan
& $gh auth status
Write-Host "`n登录完成。回到 Kilo 那边告诉我继续。" -ForegroundColor Green
pause
