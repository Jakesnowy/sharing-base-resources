@echo off
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --includeRecommended" --accept-source-agreements --accept-package-agreements
echo VS2022_EXITCODE=%ERRORLEVEL%
