@echo off
setlocal enabledelayedexpansion

:: 환경 변수 확인
if "%TRANSMITTER_PORT%"=="" (
    set TRANSMITTER_PORT=COM5
)
if "%RECEIVER_PORT%"=="" (
    set RECEIVER_PORT=COM6
)

:: 명령줄 인자 처리
if "%~1"=="" (
    echo 사용법: repl.bat [시리얼포트^|t^|r]
    echo 예시: 
    echo   repl.bat COM5    - 특정 포트에 직접 연결
    echo   repl.bat t       - 트랜스미터 포트(%TRANSMITTER_PORT%^)에 연결
    echo   repl.bat r       - 리시버 포트(%RECEIVER_PORT%^)에 연결
    exit /b 1
)

set "PORT="
if /i "%~1"=="t" (
    set "PORT=%TRANSMITTER_PORT%"
) else if /i "%~1"=="r" (
    set "PORT=%RECEIVER_PORT%"
) else (
    set "PORT=%~1"
    set "PORT=!PORT:com=COM!"
)

echo !PORT! 포트에 연결을 시도합니다...
mpremote connect !PORT! repl

endlocal 