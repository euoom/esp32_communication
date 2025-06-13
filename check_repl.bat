@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo 사용법: check_repl.bat [시리얼포트]
    echo 예시: check_repl.bat com5
    exit /b 1
)

set "PORT=%~1"
set "PORT=!PORT:com=COM!"

mpremote connect !PORT! repl

endlocal 