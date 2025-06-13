import bluetooth
from micropython import const
import struct
import time
from machine import Pin, PWM

# 시작 시 1초 딜레이
print("리시버 시작 대기 중...")
time.sleep(1)
print("리시버 시작!")

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

def bt_irq(event, data):
    """블루투스 이벤트 처리"""
    global connected, conn_handle, char_handle, count, scanning
    
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
            # 연결 성공
            conn_handle, addr_type, addr = data
            connected = True
            conn_handle = conn_handle
            print(f"트랜스미터 연결됨: {bytes(addr).hex()}")
            # 서비스 검색 시작
            ble.gattc_discover_services(conn_handle)
            
        elif event == _IRQ_PERIPHERAL_DISCONNECT:
            # 연결 해제
            conn_handle, addr_type, addr = data
            connected = False
            conn_handle = None
            char_handle = None
            count = 0  # 카운트 초기화
            led.duty_u16(0)  # LED 끄기
            print("트랜스미터 연결 해제, 재스캔 시작...")
            # 재스캔 시작
            time.sleep(0.5)  # 연결 해제 후 대기 시간 증가
            start_scan()
            
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
            # 데이터 수신
            conn_handle, value_handle, notify_data = data
            if value_handle == char_handle:
                try:
                    count += 1  # 카운트 증가
                    value = int.from_bytes(notify_data, 'little')
                    voltage = value / 4095 * 3.3  # ADC 값을 전압으로 변환
                    print(f"[반복:{count}] 수신: ADC={value:4d} (전압: {voltage:.2f}V)")
                    
                    # LED 제어 (ADC 값을 PWM 듀티로 변환)
                    duty = int((value / 4095) * 65535)  # 0-4095를 0-65535로 변환
                    led.duty_u16(duty)
                except Exception as e:
                    print(f"값 처리 실패: {e}")
    except Exception as e:
        print(f"이벤트 처리 실패: {e}")

# 블루투스 이벤트 핸들러 등록
ble.irq(bt_irq)

print("리시버 준비 완료!")
print("트랜스미터 스캔 중...")

# 스캔 시작
start_scan()

# 메인 루프
while True:
    time.sleep(0.1)  # CPU 부하 감소 