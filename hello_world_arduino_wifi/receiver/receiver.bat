@echo off
setlocal enabledelayedexpansion
chcp 65001 > nul

:: 포트 설정 파일이 있으면 읽어옵니다
if exist "..\..\set_ports.bat" (
    call "..\..\set_ports.bat"
) else (
    set RECEIVER_PORT=COM6
)

:: 보드 타입 설정
set BOARD_TYPE=esp32:esp32:Geekble_ESP32C3

cd /d %~dp0

echo [1/3] 컴파일 중... (예상 소요시간: 1분)
@REM arduino-cli compile --fqbn %BOARD_TYPE% --build-cache-path .build_cache --verbose receiver.ino
arduino-cli compile --fqbn %BOARD_TYPE% --verbose receiver.ino
if errorlevel 1 (
    echo 컴파일 실패!
    pause
    exit /b 1
)

echo [2/3] 업로드 중... (포트: %RECEIVER_PORT%, 예상 소요시간: 15초)
arduino-cli upload -p %RECEIVER_PORT% --fqbn %BOARD_TYPE% --verbose receiver.ino
if errorlevel 1 (
    echo 업로드 실패!
    pause
    exit /b 1
)

echo [3/3] 시리얼 모니터 시작... (포트: %RECEIVER_PORT%)
arduino-cli monitor -p %RECEIVER_PORT% -c baudrate=115200

:: 현재 창은 닫지 않음 (모니터 종료 시 수동으로 닫기) 