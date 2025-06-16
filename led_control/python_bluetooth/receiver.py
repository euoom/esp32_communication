import bluetooth
from micropython import const
import struct
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

# 재시도 설정
MAX_RETRY_COUNT = 5  # 최대 재시도 횟수
RETRY_INTERVAL = 5000  # 재시도 간격 (ms)
retry_count = 0  # 현재 재시도 횟수
last_retry_time = 0  # 마지막 재시도 시간

# 트랜스미터 MAC 주소 정의 (바이트 형식)
TRANSMITTER_MAC = bytes.fromhex('dc0675680b52')  # 트랜스미터 MAC 주소

# LED 설정 (PWM)
led = PWM(Pin(3))  # GPIO 3번 핀 (led_control_rules에 정의된 대로)
led.freq(1000)     # PWM 주파수 1kHz
led.duty_u16(0)    # 초기 밝기 0

# 블루투스 이벤트 상수
_IRQ_SCAN_RESULT = const(5)
_IRQ_SCAN_DONE = const(6)
_IRQ_PERIPHERAL_CONNECT = const(7)
_IRQ_PERIPHERAL_DISCONNECT = const(8)
_IRQ_GATTC_SERVICE_RESULT = const(9)
_IRQ_GATTC_SERVICE_DONE = const(10)
_IRQ_GATTC_CHARACTERISTIC_RESULT = const(11)
_IRQ_GATTC_CHARACTERISTIC_DONE = const(12)
_IRQ_GATTC_NOTIFY = const(18)

# 서비스 및 특성 UUID
_ESP32_SERVICE_UUID = bluetooth.UUID('6E400001-B5A3-F393-E0A9-E50E24DCCA9E')
_ESP32_CHAR_UUID = bluetooth.UUID('6E400002-B5A3-F393-E0A9-E50E24DCCA9E')

# BLE 클라이언트 설정
ble = bluetooth.BLE()
ble.active(False)  # 먼저 비활성화
time.sleep(0.1)    # 잠시 대기
ble.active(True)   # 다시 활성화
time.sleep(0.1)    # 활성화 대기

# 연결 상태
connected = False
conn_handle = None
char_handle = None
count = 0
scanning = False  # 스캔 상태 추적
last_scan_time = 0  # 마지막 스캔 시작 시간
SCAN_INTERVAL = 1000  # 스캔 간격 (ms)
SCAN_WINDOW = 500  # 스캔 윈도우 (ms)
SCAN_DURATION = 10000  # 스캔 지속 시간 (ms)

def start_scan():
    global scanning, last_scan_time
    current_time = time.ticks_ms()
    
    # 이전 스캔으로부터 충분한 시간이 지났는지 확인
    if not scanning and time.ticks_diff(current_time, last_scan_time) > SCAN_INTERVAL:
        try:
            # 이전 스캔이 있다면 먼저 중지
            ble.gap_scan(None)
            time.sleep(0.2)  # 스캔 중지 대기 시간 증가
            
            # 새 스캔 시작
            ble.gap_scan(SCAN_DURATION, SCAN_INTERVAL, SCAN_WINDOW)
            scanning = True
            last_scan_time = current_time
            print("스캔 시작...")
        except Exception as e:
            print(f"스캔 시작 실패: {e}")
            scanning = False
            time.sleep(0.2)  # 실패 시 대기 시간 증가
            return False
    return True

def stop_scan():
    global scanning
    if scanning:
        try:
            ble.gap_scan(None)
            scanning = False
            print("스캔 중지...")
            time.sleep(0.2)  # 스캔 중지 대기 시간 증가
        except Exception as e:
            print(f"스캔 중지 실패: {e}")

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

def cleanup_connection():
    """연결 정리"""
    global connected, conn_handle, retry_count
    connected = False
    conn_handle = None
    retry_count = 0  # 재시도 횟수 초기화
    led.duty(0)  # LED 끄기

def bt_irq(event, data):
    """블루투스 이벤트 처리"""
    global connected, conn_handle, retry_count
    try:
        if event == _IRQ_SCAN_RESULT:
            # 스캔 결과 처리
            addr_type, addr, adv_type, rssi, adv_data = data
            addr = bytes(addr)
            
            # 트랜스미터 MAC 주소와 일치하는지 확인
            if addr == TRANSMITTER_MAC:
                print(f"트랜스미터 발견! RSSI: {rssi}")
                if not connected:  # 연결 중이 아닐 때만 연결 시도
                    # 스캔 중지 후 연결 시도
                    stop_scan()
                    time.sleep(0.2)  # 연결 전 대기 시간 증가
                    try:
                        ble.gap_connect(addr_type, addr)
                        print("트랜스미터 연결 시도 중...")
                    except Exception as e:
                        print(f"연결 시도 실패: {e}")
                        time.sleep(0.2)
                        start_scan()
            else:
                # 디버깅용: 다른 장치 발견 시 로그
                if rssi > -50:  # RSSI가 -50dBm보다 강한 경우만 출력
                    print(f"다른 장치 발견: {addr.hex()} (RSSI: {rssi})")
                
        elif event == _IRQ_SCAN_DONE:
            # 스캔 완료 후 재시작 (연결되지 않은 경우에만)
            scanning = False
            if not connected:
                print("스캔 완료, 재시작...")
                time.sleep(0.2)  # 재스캔 전 대기 시간 증가
                start_scan()
            else:
                print("스캔 완료 (연결 유지 중)")
                
        elif event == _IRQ_PERIPHERAL_CONNECT:
            # 서버 연결
            conn_handle, addr_type, addr = data
            connected = True
            retry_count = 0  # 연결 성공 시 재시도 횟수 초기화
            print(f"서버 연결됨: {bytes(addr).hex()}")
            
        elif event == _IRQ_PERIPHERAL_DISCONNECT:
            # 서버 연결 해제
            conn_handle, addr_type, addr = data
            cleanup_connection()
            print("서버 연결 해제")
            
        elif event == _IRQ_GATTC_SERVICE_RESULT:
            # 서비스 검색 결과
            conn_handle, start_handle, end_handle, uuid = data
            if uuid == _ESP32_SERVICE_UUID:
                print(f"서비스 발견: {uuid}")
                # 특성 검색
                ble.gattc_discover_characteristics(conn_handle, start_handle, end_handle)
                
        elif event == _IRQ_GATTC_CHARACTERISTIC_RESULT:
            # 특성 검색 결과
            conn_handle, def_handle, value_handle, properties, uuid = data
            if uuid == _ESP32_CHAR_UUID:
                print(f"특성 발견: {uuid}")
                char_handle = value_handle
                # 알림 활성화
                ble.gattc_write(conn_handle, value_handle + 1, bytes([0x01, 0x00]), 1)
                print("알림 활성화됨")
                
        elif event == _IRQ_GATTC_NOTIFY:
            # 서버로부터 데이터 수신
            conn_handle, value_handle, notify_data = data
            if conn_handle == conn_handle:  # 연결된 서버로부터의 데이터인지 확인
                try:
                    # 2바이트 정수로 데이터 파싱
                    value = struct.unpack("<H", notify_data)[0]
                    set_pwm_value(value)
                except Exception as e:
                    print(f"데이터 파싱 오류: {e}")
    except Exception as e:
        print(f"이벤트 처리 중 오류 발생: {e}")

# 블루투스 이벤트 핸들러 등록
ble.irq(bt_irq)

print("리시버 준비 완료!")
print("트랜스미터 스캔 중...")

# 스캔 시작
start_scan()

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    # 연결이 끊어진 경우 재연결 시도
    if not connected:
        if check_retry_limit():
            continue
            
        # 재시도 간격 확인
        if time.ticks_diff(current_time, last_retry_time) < RETRY_INTERVAL:
            continue
            
        retry_count += 1
        last_retry_time = current_time
        print(f"연결 시도 실패 (시도 {retry_count}/{MAX_RETRY_COUNT})")
        
        # 서버 스캔 및 연결 시도
        try:
            scan_result = ble.gap_scan(2000, 30000, 30000)
            if scan_result:
                addr_type, addr, adv_type, rssi, adv_data = scan_result
                if b"ESP32_LED_BLE" in adv_data:
                    print(f"서버 발견: {bytes(addr).hex()}")
                    ble.gap_connect(addr_type, addr)
        except Exception as e:
            print(f"스캔/연결 시도 중 오류 발생: {e}")
            cleanup_connection()
    
    time.sleep(0.1)  # CPU 부하 감소 