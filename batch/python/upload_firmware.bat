@echo off
setlocal enabledelayedexpansion

:: 환경 변수 확인
if "%TRANSMITTER_PORT%"=="" (
    set TRANSMITTER_PORT=COM5
)
if "%RECEIVER_PORT%"=="" (
    set RECEIVER_PORT=COM6
)

echo.
echo === 펌웨어 업로드 시작 ===
echo.

if "%~1"=="" (
    echo 사용법: upload_firmware.bat [시리얼포트^|t^|r]
    echo 예시: 
    echo   upload_firmware.bat COM5    - 특정 포트에 직접 업로드
    echo   upload_firmware.bat t       - 트랜스미터 포트(%TRANSMITTER_PORT%^)에 업로드
    echo   upload_firmware.bat r       - 리시버 포트(%RECEIVER_PORT%^)에 업로드
    exit /b 1
)

:: 포트 설정
set "PORT="
if /i "%~1"=="t" (
    set "PORT=%TRANSMITTER_PORT%"
) else if /i "%~1"=="r" (
    set "PORT=%RECEIVER_PORT%"
) else (
    set "PORT=%~1"
    set "PORT=!PORT:com=COM!"
)

:: 펌웨어 경로 설정
set FIRMWARE=ESP32_GENERIC_C3-20250415-v1.25.0.bin

echo.
echo 현재 설정:
echo - 포트: !PORT!
echo - 펌웨어: %FIRMWARE%
echo.
echo 주의: 이 작업은 기존 펌웨어를 삭제합니다.
echo 계속하시겠습니까? (Y/n)
set /p CONFIRM=Y

if /i "!CONFIRM!"=="n" (
    echo 작업이 취소되었습니다.
    exit /b
)

echo.
echo 1. 기존 펌웨어 삭제 중...
python -m esptool --port !PORT! erase_flash
if errorlevel 1 (
    echo 펌웨어 삭제 실패!
    exit /b 1
)

echo.
echo 2. MicroPython 펌웨어 업로드 중...
python -m esptool --port !PORT! --baud 460800 write_flash -z 0x0 %FIRMWARE%
if errorlevel 1 (
    echo 펌웨어 업로드 실패!
    exit /b 1
)

echo.
echo 펌웨어 업로드가 완료되었습니다!
echo REPL 모드를 확인하려면 repl.bat를 실행하세요.

echo.
echo === 펌웨어 업로드 완료 ===
echo.

endlocal 