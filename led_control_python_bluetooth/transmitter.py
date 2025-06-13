import bluetooth
from micropython import const
import struct
import time
from machine import Pin, ADC

# 시작 시 1초 딜레이
print("트랜스미터 시작 대기 중...")
time.sleep(1)
print("트랜스미터 시작!")

# ADC 설정
adc = ADC(Pin(3))  # GPIO 3번 핀 (led_control_rules에 정의된 대로)
adc.atten(ADC.ATTN_11DB)  # 0-3.3V 범위
adc.width(ADC.WIDTH_12BIT)  # 12비트 해상도 명시적 설정

# BLE 이벤트 상수
_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_WRITE = const(3)
_IRQ_GATTS_READ_REQUEST = const(4)
_IRQ_SCAN_RESULT = const(5)
_IRQ_SCAN_DONE = const(6)
_IRQ_PERIPHERAL_CONNECT = const(7)
_IRQ_PERIPHERAL_DISCONNECT = const(8)
_IRQ_GATTC_SERVICE_RESULT = const(9)
_IRQ_GATTC_CHARACTERISTIC_RESULT = const(11)
_IRQ_GATTC_DESCRIPTOR_RESULT = const(13)
_IRQ_GATTC_READ_RESULT = const(15)
_IRQ_GATTC_READ_DONE = const(16)
_IRQ_GATTC_WRITE_DONE = const(17)
_IRQ_GATTC_NOTIFY = const(18)

# BLE 서비스와 특성 UUID (UUID 클래스 사용)
SERVICE_UUID = bluetooth.UUID('6E400001-B5A3-F393-E0A9-E50E24DCCA9E')
CHARACTERISTIC_UUID = bluetooth.UUID('6E400002-B5A3-F393-E0A9-E50E24DCCA9E')

# BLE 서버 설정
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
last_send_time = 0  # 마지막 메시지 전송 시간
SEND_INTERVAL = 5000  # 메시지 전송 간격 (ms) - WiFi 버전과 동일하게 5초

def bt_irq(event, data):
    """블루투스 이벤트 처리"""
    global connected, conn_handle, char_handle, count
    try:
        if event == _IRQ_CENTRAL_CONNECT:
            # 클라이언트 연결
            conn_handle, addr_type, addr = data
            connected = True
            print(f"클라이언트 연결됨: {bytes(addr).hex()}")
            
        elif event == _IRQ_CENTRAL_DISCONNECT:
            # 클라이언트 연결 해제
            conn_handle, addr_type, addr = data
            connected = False
            conn_handle = None
            char_handle = None
            print("클라이언트 연결 해제")
            # 연결 해제 후 광고 재시작
            time.sleep(0.1)
            ble.gap_advertise(100000, b"ESP32_LED_BLE")
            
        elif event == _IRQ_GATTS_WRITE:
            # 클라이언트로부터 데이터 수신 (필요한 경우)
            conn_handle, attr_handle = data
            if attr_handle == char_handle:
                received_data = ble.gatts_read(char_handle)
                print(f"데이터 수신됨: {received_data}")
                
    except Exception as e:
        print(f"이벤트 처리 중 오류 발생: {e}")

# BLE 콜백 설정
ble.irq(bt_irq)

# 서비스와 특성 생성
services = (
    (SERVICE_UUID, (
        (CHARACTERISTIC_UUID, bluetooth.FLAG_NOTIFY | bluetooth.FLAG_WRITE),
    )),
)

# 서비스 등록
((char_handle,),) = ble.gatts_register_services(services)
print(f"서비스 등록됨 (특성 핸들: {char_handle})")

# 광고 시작
print("블루투스 광고 시작...")
ble.gap_advertise(100000, b"ESP32_LED_BLE")

print("트랜스미터 준비 완료!")
print("클라이언트 연결 대기 중...")

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    # 5초마다 ADC 값 전송 (WiFi 버전과 동일한 간격)
    if connected and time.ticks_diff(current_time, last_send_time) > SEND_INTERVAL:
        try:
            # ADC 값 읽기 (0-4095)
            value = adc.read()  # 12비트 ADC 값 직접 읽기
            voltage = value / 4095 * 3.3  # ADC 값을 전압으로 변환
            count += 1
            print(f"[반복:{count}] 전송: ADC={value:4d} (전압: {voltage:.2f}V)")
            
            # 값 전송 (2바이트 정수로)
            ble.gatts_notify(conn_handle, char_handle, struct.pack("<H", value))
            last_send_time = current_time
        except Exception as e:
            print(f"값 전송 실패: {e}")
    
    time.sleep(0.1)  # CPU 부하 감소 (WiFi 버전과 동일)

if __name__ == "__main__":
    main() 