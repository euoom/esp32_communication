@echo off
setlocal enabledelayedexpansion

echo.
echo === ESP32 MicroPython Receiver Setup ===
echo.

REM 보드 재설정
echo 1. Resetting receiver board...
echo Port: COM6
echo.

REM 보드 재설정
mpremote connect COM6 soft-reset
if errorlevel 1 (
    echo Failed to reset board, trying hard reset...
    mpremote connect COM6 reset
    timeout /t 2 /nobreak > nul
)

REM 파일 업로드
echo 2. Uploading receiver code...
echo Port: COM6
echo.

REM 파일 전송 (재시도 로직 추가)
set MAX_RETRIES=3
set RETRY_COUNT=0

:RETRY_UPLOAD
set /a RETRY_COUNT+=1
echo Upload attempt !RETRY_COUNT! of !MAX_RETRIES!

REM 파일 전송
mpremote connect COM6 cp receiver.py :main.py
if errorlevel 1 (
    if !RETRY_COUNT! lss !MAX_RETRIES! (
        echo Upload failed, retrying after reset...
        mpremote connect COM6 soft-reset
        timeout /t 2 /nobreak > nul
        goto RETRY_UPLOAD
    ) else (
        echo Failed to upload receiver code after !MAX_RETRIES! attempts!
        exit /b 1
    )
)

echo Receiver code uploaded successfully!
echo.

REM 모니터링 시작
echo 3. Starting serial monitor...
echo Port: COM6
echo Baudrate: 115200
echo.
echo Press Ctrl+C to stop monitoring
echo.

REM 모니터링 시작
mpremote connect COM6 repl

endlocal 