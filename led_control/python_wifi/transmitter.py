import network
import socket
import time
from machine import Pin, ADC
import array

# 시작 시 1초 딜레이
print("트랜스미터 시작 대기 중...")
time.sleep(1)
print("트랜스미터 시작!")

# ADC 설정
adc = ADC(Pin(3))  # GPIO 3번 핀
adc.atten(ADC.ATTN_11DB)  # 0-3.3V 범위
adc.width(ADC.WIDTH_12BIT)  # 12비트 해상도

# ADC 필터링 설정
ADC_SAMPLES = 5  # 이동 평균 샘플 수
adc_values = array.array('i', [0] * ADC_SAMPLES)  # ADC 값 저장 배열
adc_index = 0  # 현재 ADC 샘플 인덱스

# WiFi AP 설정
SSID = "ESP32_LED_AP"
PASSWORD = "12345678"
PORT = 8080

# 재시도 설정
MAX_RETRY_COUNT = 5  # 최대 재시도 횟수
RETRY_INTERVAL = 5000  # 재시도 간격 (ms)
retry_count = 0  # 현재 재시도 횟수
last_retry_time = 0  # 마지막 재시도 시간

# 전송 설정
SEND_INTERVAL = 5000  # 데이터 전송 간격 (ms)
last_send_time = 0  # 마지막 전송 시간

def get_filtered_adc():
    """ADC 값 필터링 (이동 평균)"""
    global adc_index
    
    # 새로운 ADC 값 읽기
    adc_values[adc_index] = adc.read()
    adc_index = (adc_index + 1) % ADC_SAMPLES
    
    # 이동 평균 계산
    return sum(adc_values) // ADC_SAMPLES

def check_retry_limit():
    """재시도 제한 확인"""
    global retry_count
    if retry_count >= MAX_RETRY_COUNT:
        print("최대 재시도 횟수 초과. 장치를 재시작합니다.")
        time.sleep(1)
        import machine
        machine.reset()
        return True
    return False

def cleanup_connection(client):
    """연결 정리"""
    global retry_count
    try:
        client.close()
    except:
        pass
    retry_count = 0  # 재시도 횟수 초기화

def setup_wifi_ap():
    """WiFi AP 설정"""
    ap = network.WLAN(network.AP_IF)
    ap.active(True)
    ap.config(essid=SSID, password=PASSWORD)
    while not ap.active():
        time.sleep(0.1)
    print(f"WiFi AP 시작됨: {SSID}")
    print(f"IP 주소: {ap.ifconfig()[0]}")

def handle_client(client):
    """클라이언트 연결 처리"""
    global last_send_time, retry_count
    count = 0
    
    try:
        while True:
            current_time = time.ticks_ms()
            
            # 5초마다 ADC 값 전송
            if time.ticks_diff(current_time, last_send_time) > SEND_INTERVAL:
                try:
                    # 필터링된 ADC 값 읽기
                    value = get_filtered_adc()
                    voltage = value / 4095 * 3.3  # ADC 값을 전압으로 변환
                    count += 1
                    print(f"[반복:{count}] 전송: ADC={value:4d} (전압: {voltage:.2f}V)")
                    
                    # 값 전송 (2바이트 정수로)
                    client.send(value.to_bytes(2, 'little'))
                    last_send_time = current_time
                except Exception as e:
                    print(f"값 전송 실패: {e}")
                    return False
                    
            time.sleep(0.1)  # CPU 부하 감소
            
    except Exception as e:
        print(f"클라이언트 처리 중 오류 발생: {e}")
        return False
        
    return True

# WiFi AP 시작
setup_wifi_ap()

# 서버 소켓 생성
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(('0.0.0.0', PORT))
server.listen(1)
print(f"서버 시작됨 (포트: {PORT})")

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    try:
        # 클라이언트 연결 수락
        client, addr = server.accept()
        print(f"클라이언트 연결됨: {addr}")
        
        # 연결 성공 시 재시도 횟수 초기화
        retry_count = 0
        
        # 클라이언트 처리
        if not handle_client(client):
            cleanup_connection(client)
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
    
    time.sleep(0.1)  # CPU 부하 감소 