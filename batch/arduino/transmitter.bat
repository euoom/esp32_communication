@echo off
setlocal enabledelayedexpansion
chcp 65001 > nul

:: 환경 변수 확인
if "%TRANSMITTER_PORT%"=="" (
    set TRANSMITTER_PORT=COM5
)

:: 보드 타입 설정
set BOARD_TYPE=esp32:esp32:Geekble_ESP32C3

:: 명령줄 인자 확인
if "%~1"=="" (
    echo 사용법: transmitter_arduino.bat [ino파일경로]
    echo 예시: transmitter_arduino.bat ..\hello_world\arduino_bluetooth\transmitter\transmitter.ino
    exit /b 1
)

set "INO_FILE=%~1"
if not exist "!INO_FILE!" (
    echo 오류: !INO_FILE! 파일을 찾을 수 없습니다.
    exit /b 1
)

:: 작업 디렉토리로 이동
cd /d "%~dp0"

echo.
echo === Arduino 스케치 업로드 시작 ===
echo 현재 설정:
echo - 포트: %TRANSMITTER_PORT%
echo - 보드: %BOARD_TYPE%
echo - 스케치: !INO_FILE!
echo.

echo [1/3] 컴파일 중... (예상 소요시간: 1분)
arduino-cli compile --fqbn %BOARD_TYPE% --verbose "!INO_FILE!"
if errorlevel 1 (
    echo 컴파일 실패!
    pause
    exit /b 1
)

echo.
echo [2/3] 업로드 중... (포트: %TRANSMITTER_PORT%, 예상 소요시간: 15초)
arduino-cli upload -p %TRANSMITTER_PORT% --fqbn %BOARD_TYPE% --verbose "!INO_FILE!"
if errorlevel 1 (
    echo 업로드 실패!
    pause
    exit /b 1
)

echo.
echo [3/3] 시리얼 모니터 시작... (포트: %TRANSMITTER_PORT%)
echo 모니터를 종료하려면 Ctrl+C를 누르세요.
echo.
arduino-cli monitor -p %TRANSMITTER_PORT% -c baudrate=115200

endlocal 