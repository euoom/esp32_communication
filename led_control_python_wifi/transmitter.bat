@echo off
setlocal enabledelayedexpansion

echo.
echo === ESP32 MicroPython Transmitter Setup ===
echo.

REM 보드 재설정
echo 1. Resetting transmitter board...
echo Port: COM5
echo.

REM 보드 재설정
mpremote connect COM5 soft-reset
if errorlevel 1 (
    echo Failed to reset board, trying hard reset...
    mpremote connect COM5 reset
    timeout /t 2 /nobreak > nul
)

REM 파일 업로드
echo 2. Uploading transmitter code...
echo Port: COM5
echo.

REM 파일 전송 (재시도 로직 추가)
set MAX_RETRIES=3
set RETRY_COUNT=0

:RETRY_UPLOAD
set /a RETRY_COUNT+=1
echo Upload attempt !RETRY_COUNT! of !MAX_RETRIES!

REM 파일 전송 (transmitter.py를 main.py로 업로드)
mpremote connect COM5 cp transmitter.py :main.py
if errorlevel 1 (
    if !RETRY_COUNT! lss !MAX_RETRIES! (
        echo Upload failed, retrying after reset...
        mpremote connect COM5 soft-reset
        timeout /t 2 /nobreak > nul
        goto RETRY_UPLOAD
    ) else (
        echo Failed to upload transmitter code after !MAX_RETRIES! attempts!
        exit /b 1
    )
)

echo Transmitter code uploaded successfully!
echo.

REM 모니터링 시작
echo 3. Starting serial monitor...
echo Port: COM5
echo Baudrate: 115200
echo.
echo 실행 방법:
echo 1. repl 모드에서 다음 명령어를 입력하세요:
echo    import main
echo.
echo Press Ctrl+C to stop monitoring
echo.

REM 모니터링 시작
mpremote connect COM5 repl

endlocal 