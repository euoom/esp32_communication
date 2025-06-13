@echo off
echo 아두이노 CLI 설치 시작...
echo 이 설치 프로그램은 관리자 권한이 필요합니다.
echo 관리자 권한으로 실행되지 않은 경우 설치가 실패할 수 있습니다.
echo.

REM 설치 디렉토리 설정
set "INSTALL_DIR=C:\Program Files\Arduino CLI"
set DOWNLOAD_URL=https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip

echo 설치 디렉토리: %INSTALL_DIR%

REM 디렉토리 생성
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

REM 아두이노 CLI 다운로드
echo 아두이노 CLI 다운로드 중...
powershell -Command "& {Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%INSTALL_DIR%\arduino-cli.zip'}"

REM 압축 해제
echo 압축 해제 중...
powershell -Command "& {Expand-Archive -Path '%INSTALL_DIR%\arduino-cli.zip' -DestinationPath '%INSTALL_DIR%' -Force}"

REM 임시 파일 삭제
del "%INSTALL_DIR%\arduino-cli.zip"

REM PATH 환경 변수에 추가
setx PATH "%PATH%;"%INSTALL_DIR%""

echo.
echo 아두이노 CLI 설치가 완료되었습니다.
echo 시스템을 재시작하거나 새 명령 프롬프트 창을 열어주세요.
echo.
echo 설치 후 다음 명령어로 확인할 수 있습니다:
echo arduino-cli version
echo arduino-cli board list
echo.
pause 