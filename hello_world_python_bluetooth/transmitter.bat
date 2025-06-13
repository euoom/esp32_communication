@echo off
setlocal enabledelayedexpansion

echo.
echo === ESP32 MicroPython Transmitter Setup ===
echo.

REM Upload transmitter code
echo 1. Uploading transmitter code...
echo Port: COM5
echo.

REM File transfer
mpremote connect COM5 cp transmitter.py :main.py
if errorlevel 1 (
    echo Failed to upload transmitter code!
    exit /b 1
)

echo Transmitter code uploaded successfully!
echo.

REM Start monitoring
echo 2. Starting serial monitor...
echo Port: COM5
echo Baudrate: 115200
echo.
echo Press Ctrl+C to stop monitoring
echo.

REM Start monitoring with mpremote
mpremote connect COM5 repl

endlocal 