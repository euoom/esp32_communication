@echo off
setlocal enabledelayedexpansion
chcp 65001 > nul

:: 현재 스크립트의 디렉토리로 이동
cd /d "%~dp0"

:: 공통 배치 파일 호출
call "%~dp0..\..\..\..\batch\python\transmitter.bat" "%~dp0transmitter.py"

endlocal 