@echo off
set HTTPS_PROXY=http://127.0.0.1:7897
set HTTP_PROXY=http://127.0.0.1:7897
set "PATH=%PATH%;C:\Program Files\GitHub CLI"
"C:\Program Files\GitHub CLI\gh.exe" auth login --hostname github.com --git-protocol https --web
echo EXITCODE=%ERRORLEVEL%
