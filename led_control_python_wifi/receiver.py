import network
import socket
import time
from machine import Pin, PWM

# 시작 시 1초 딜레이
print("리시버 시작 대기 중...")
time.sleep(1)
print("리시버 시작!")

# WiFi 설정
WIFI_SSID = "ESP32_LED_AP"  # 트랜스미터의 AP 이름
WIFI_PASSWORD = "12345678"  # 트랜스미터의 AP 비밀번호
PORT = 8080  # 통신 포트

# LED 설정 (PWM)
led = PWM(Pin(3))  # GPIO 3번 핀
led.freq(1000)     # PWM 주파수 1kHz
led.duty_u16(0)    # 초기 밝기 0

# WiFi 스테이션 모드 설정
wlan = network.WLAN(network.STA_IF)
wlan.active(True)

# 연결 상태
connected = False
sock = None
last_server_time = 0  # 마지막 통신 시간
count = 0  # 반복 카운트 추가

def cleanup_connection():
    """연결 정리"""
    global connected, sock, last_server_time, count
    
    if sock:
        try:
            sock.close()
        except:
            pass
        sock = None
    
    connected = False
    last_server_time = 0
    count = 0  # 카운트 초기화
    led.duty_u16(0)  # LED 끄기

def connect_to_transmitter():
    """트랜스미터에 연결"""
    global connected, sock, last_server_time
    
    try:
        # 이전 연결이 있다면 정리
        cleanup_connection()
        
        # WiFi 연결 상태 확인
        if wlan.isconnected():
            print("기존 WiFi 연결 해제 중...")
            wlan.disconnect()
            time.sleep(1)
        
        # 트랜스미터 AP에 연결
        print("트랜스미터 AP에 연결 중...")
        print(f"AP 정보: SSID={WIFI_SSID}, Password={WIFI_PASSWORD}")
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        
        # 연결 대기 (최대 30초)
        max_wait = 30
        while max_wait > 0:
            status = wlan.status()
            if status == network.STAT_GOT_IP:
                break
            elif status == network.STAT_CONNECTING:
                print(f"연결 중... {max_wait}초 남음")
            elif status == network.STAT_CONNECT_FAIL:
                print("연결 실패")
                return False
            elif status == network.STAT_NO_AP_FOUND:
                print("AP를 찾을 수 없음")
                return False
            max_wait -= 1
            time.sleep(1)
        
        if not wlan.isconnected():
            print("WiFi 연결 실패")
            return False
            
        # IP 정보 출력
        ip_info = wlan.ifconfig()
        print(f"WiFi 연결됨: {ip_info}")
        
        # 서버 IP는 게이트웨이 IP (AP의 IP)
        server_ip = ip_info[2]  # 게이트웨이 IP
        print(f"트랜스미터 IP: {server_ip}")
        
        # 소켓 생성 및 연결 (최대 3번 시도)
        for attempt in range(3):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)  # 5초 타임아웃
                
                print(f"트랜스미터({server_ip}:{PORT})에 연결 시도 중... (시도 {attempt + 1}/3)")
                sock.connect((server_ip, PORT))
                
                connected = True
                last_server_time = time.ticks_ms()
                print(f"트랜스미터({server_ip}:{PORT})에 연결됨")
                return True
                
            except Exception as e:
                print(f"연결 시도 {attempt + 1} 실패: {e}")
                if sock:
                    try:
                        sock.close()
                    except:
                        pass
                    sock = None
                if attempt < 2:  # 마지막 시도가 아니면 대기 후 재시도
                    print("3초 후 재시도...")
                    time.sleep(3)
        
        print("모든 연결 시도 실패")
        return False
        
    except Exception as e:
        print(f"연결 중 예외 발생: {e}")
        cleanup_connection()
        return False

def receive_and_control():
    """값 수신 및 LED 제어"""
    global sock, connected, last_server_time, count
    
    try:
        if not connected or not sock:
            print("연결이 끊어짐")
            cleanup_connection()
            return False
            
        # 값 수신
        data = sock.recv(1024)
        if data:
            try:
                count += 1  # 카운트 증가
                value = int(data.decode())
                voltage = value / 4095 * 3.3  # ADC 값을 전압으로 변환
                print(f"[반복:{count}] 수신: ADC={value:4d} (전압: {voltage:.2f}V)")
                last_server_time = time.ticks_ms()  # 수신 시간 갱신
                
                # LED 제어 (ADC 값을 PWM 듀티로 변환)
                duty = int((value / 4095) * 65535)  # 0-4095를 0-65535로 변환
                led.duty_u16(duty)
                return True
            except ValueError:
                print(f"잘못된 데이터 수신: {data}")
    except OSError as e:
        if "timed out" in str(e):  # 타임아웃은 일반적인 상황
            pass
        else:
            print(f"값 수신 실패: {e}")
            cleanup_connection()
    except Exception as e:
        print(f"값 수신 실패: {e}")
        cleanup_connection()
    return False

print("WiFi 클라이언트 시작...")

# 메인 루프
while True:
    # 연결이 끊어진 경우 재연결
    if not connected:
        if connect_to_transmitter():
            print("연결 성공, 2초 대기 후 데이터 수신 시작")
            time.sleep(2)  # 연결 후 대기 시간
        else:
            print("재연결 시도 중... 10초 후 다시 시도합니다.")
            time.sleep(10)  # 재연결 실패 시 대기 시간
        continue
    
    # 값 수신 및 LED 제어
    receive_and_control()
    
    time.sleep(0.1)  # CPU 부하 감소 