import network
import socket
import time
from machine import Pin, PWM
import array

# 시작 시 1초 딜레이
print("리시버 시작 대기 중...")
time.sleep(1)
print("리시버 시작!")

# PWM 설정
led = PWM(Pin(3), freq=5000, duty=0)  # GPIO 3번 핀, 5kHz 주파수
PWM_RESOLUTION = 8  # 8비트 PWM 해상도
PWM_MAX = 255  # 최대 PWM 값
PWM_MIN = 0  # 최소 PWM 값
PWM_THRESHOLD = 10  # PWM 값 변화 임계값

# PWM 노이즈 필터링
last_pwm_value = 0  # 마지막 PWM 값

# WiFi 설정
SSID = "ESP32_LED_AP"
PASSWORD = "12345678"
HOST = "192.168.4.1"  # AP의 기본 IP 주소
PORT = 8080

# 재시도 설정
MAX_RETRY_COUNT = 5  # 최대 재시도 횟수
RETRY_INTERVAL = 5000  # 재시도 간격 (ms)
retry_count = 0  # 현재 재시도 횟수
last_retry_time = 0  # 마지막 재시도 시간

def set_pwm_value(value):
    """PWM 값 설정 (노이즈 필터링 포함)"""
    global last_pwm_value
    
    # ADC 값을 PWM 값으로 변환 (0-4095 -> 0-255)
    pwm_value = int((value / 4095) * PWM_MAX)
    
    # 노이즈 필터링: 임계값보다 작은 변화는 무시
    if abs(pwm_value - last_pwm_value) < PWM_THRESHOLD:
        return
        
    # PWM 값 범위 제한
    pwm_value = max(PWM_MIN, min(PWM_MAX, pwm_value))
    
    # PWM 값 업데이트
    led.duty(pwm_value)
    last_pwm_value = pwm_value
    print(f"PWM 값 설정: {pwm_value}")

def check_retry_limit():
    """재시도 제한 확인"""
    global retry_count
    if retry_count >= MAX_RETRY_COUNT:
        print("최대 재시도 횟수 초과. 장치를 재시작합니다.")
        led.duty(0)  # LED 끄기
        time.sleep(1)
        import machine
        machine.reset()
        return True
    return False

def cleanup_connection(sock):
    """연결 정리"""
    global retry_count
    try:
        sock.close()
    except:
        pass
    retry_count = 0  # 재시도 횟수 초기화
    led.duty(0)  # LED 끄기

def connect_to_wifi():
    """WiFi 연결"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print(f"WiFi 연결 시도: {SSID}")
        wlan.connect(SSID, PASSWORD)
        
        # 연결 대기
        max_wait = 10
        while max_wait > 0:
            if wlan.isconnected():
                break
            max_wait -= 1
            print("연결 대기 중...")
            time.sleep(1)
            
    if wlan.isconnected():
        print("WiFi 연결됨")
        print(f"IP 주소: {wlan.ifconfig()[0]}")
        return True
    else:
        print("WiFi 연결 실패")
        return False

def connect_to_server():
    """서버 연결"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 5초 타임아웃
        sock.connect((HOST, PORT))
        print(f"서버 연결됨: {HOST}:{PORT}")
        return sock
    except Exception as e:
        print(f"서버 연결 실패: {e}")
        return None

def receive_and_control(sock):
    """데이터 수신 및 LED 제어"""
    global retry_count
    count = 0
    
    try:
        while True:
            # 데이터 수신 (2바이트)
            data = sock.recv(2)
            if not data:
                print("연결이 끊어짐")
                return False
                
            try:
                # 2바이트 정수로 데이터 파싱
                value = int.from_bytes(data, 'little')
                voltage = value / 4095 * 3.3  # ADC 값을 전압으로 변환
                count += 1
                print(f"[반복:{count}] 수신: ADC={value:4d} (전압: {voltage:.2f}V)")
                
                # LED 제어
                set_pwm_value(value)
                
            except Exception as e:
                print(f"데이터 처리 실패: {e}")
                return False
                
    except Exception as e:
        print(f"데이터 수신 중 오류 발생: {e}")
        return False
        
    return True

# WiFi 연결
if not connect_to_wifi():
    print("WiFi 연결 실패, 재시작 중...")
    time.sleep(5)
    import machine
    machine.reset()

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    try:
        # 서버 연결
        sock = connect_to_server()
        if sock:
            # 연결 성공 시 재시도 횟수 초기화
            retry_count = 0
            
            # 데이터 수신 및 LED 제어
            if not receive_and_control(sock):
                cleanup_connection(sock)
                continue
                
    except Exception as e:
        print(f"연결 처리 중 오류 발생: {e}")
        
        # 재시도 제한 확인
        if check_retry_limit():
            continue
            
        # 재시도 간격 확인
        if time.ticks_diff(current_time, last_retry_time) < RETRY_INTERVAL:
            continue
            
        retry_count += 1
        last_retry_time = current_time
        print(f"연결 시도 실패 (시도 {retry_count}/{MAX_RETRY_COUNT})")
        
        # WiFi 재연결 시도
        if not connect_to_wifi():
            cleanup_connection(sock)
            continue
    
    time.sleep(0.1)  # CPU 부하 감소 