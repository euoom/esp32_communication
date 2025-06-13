@echo off
setlocal enabledelayedexpansion

:: ESP32 보드 설정
set BOARD_TYPE=esp32:esp32:Geekble_ESP32C3
set BAUD_RATE=115200

:: 환경 변수 확인
if "%TRANSMITTER_PORT%"=="" (
    set TRANSMITTER_PORT=COM5
    setx TRANSMITTER_PORT "COM5"
)
if "%RECEIVER_PORT%"=="" (
    set RECEIVER_PORT=COM6
    setx RECEIVER_PORT "COM6"
)

:: 현재 설정된 포트 출력
echo 현재 설정된 포트:
echo TRANSMITTER_PORT: %TRANSMITTER_PORT%
echo RECEIVER_PORT: %RECEIVER_PORT%
echo.

:: Arduino CLI 환경 변수 설정
set ARDUINO_PORT=%TRANSMITTER_PORT%
set ARDUINO_BAUD_RATE=%BAUD_RATE%
set ARDUINO_BOARD_TYPE=%BOARD_TYPE%

:: 설정된 값 출력
echo ===== ESP32 설정 정보 =====
echo 시리얼 포트: %ARDUINO_PORT%
echo 보드 타입: %ARDUINO_BOARD_TYPE%
echo 통신 속도: %ARDUINO_BAUD_RATE%
echo =========================
echo.

:: 환경 변수 영구 설정
setx ARDUINO_PORT "%ARDUINO_PORT%"
setx ARDUINO_BAUD_RATE "%ARDUINO_BAUD_RATE%"
setx ARDUINO_BOARD_TYPE "%ARDUINO_BOARD_TYPE%"

echo 환경 변수가 설정되었습니다.
echo 이제 arduino-cli 명령어에서 %ARDUINO_PORT%를 사용할 수 있습니다.

endlocal 