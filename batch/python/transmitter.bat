@echo off
setlocal enabledelayedexpansion
chcp 65001 > nul

:: 환경 변수 확인
if "%TRANSMITTER_PORT%"=="" (
    set TRANSMITTER_PORT=COM5
)

:: 명령줄 인자 확인
if "%~1"=="" (
    echo 사용법: transmitter.bat [파이썬파일경로]
    echo 예시: transmitter.bat ..\hello_world\python_bluetooth\transmitter\transmitter.py
    exit /b 1
)

set "PY_FILE=%~1"
if not exist "!PY_FILE!" (
    echo 오류: !PY_FILE! 파일을 찾을 수 없습니다.
    exit /b 1
)

:: 작업 디렉토리로 이동
cd /d "%~dp0"

echo.
echo === MicroPython 스크립트 업로드 시작 ===
echo 현재 설정:
echo - 포트: %TRANSMITTER_PORT%
echo - 스크립트: !PY_FILE!
echo.

echo [1/2] 스크립트 업로드 중... (예상 소요시간: 5초)
mpremote connect %TRANSMITTER_PORT% cp "!PY_FILE!" :main.py
if errorlevel 1 (
    echo 업로드 실패!
    pause
    exit /b 1
)

echo.
echo [2/2] 시리얼 모니터 시작... (포트: %TRANSMITTER_PORT%)
echo 모니터를 종료하려면 Ctrl+C를 누르세요.
echo.
mpremote connect %TRANSMITTER_PORT% repl

endlocal 