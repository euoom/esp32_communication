import bluetooth
from micropython import const
import struct
import time
from machine import Pin

# 시작 시 1초 딜레이
print("트랜스미터 시작 대기 중...")
time.sleep(1)
print("트랜스미터 시작!")

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
SERVICE_UUID = bluetooth.UUID("55072829-bc9e-4c53-938a-74a6d4c78776")
CHARACTERISTIC_UUID = bluetooth.UUID("55072829-bc9e-4c53-938a-74a6d4c78777")  # 마지막 숫자만 다름

# BLE 서버 설정
ble = bluetooth.BLE()
ble.active(True)

# 연결 상태
connected = False
conn_handle = None
char_handle = None
count = 0
last_send_time = 0  # 마지막 메시지 전송 시간
SEND_INTERVAL = 5000  # 메시지 전송 간격 (ms)
current_count = 0  # 현재 메시지의 반복 카운트

def bt_irq(event, data):
    global connected, conn_handle, char_handle, count, current_count
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
            
        elif event == _IRQ_GATTS_WRITE:
            # 클라이언트로부터 데이터 수신
            conn_handle, attr_handle = data
            if attr_handle == char_handle:
                received_data = ble.gatts_read(char_handle)
                message = received_data.decode()
                print(f"[반복:{current_count} | 진행:4/4] 수신한 메시지 : {message}")
                
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

print("블루투스 서버 시작...")
print("클라이언트 연결 대기 중...")

# 광고 시작
ble.gap_advertise(100000, b"ESP32_BLE_Server")

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    # 5초마다 메시지 전송
    if connected and time.ticks_diff(current_time, last_send_time) > SEND_INTERVAL:
        count += 1
        current_count = count  # 현재 메시지의 반복 카운트 저장
        message = "hello"
        print(f"[반복:{current_count} | 진행:1/4] 전송할 메시지 : {message}")
        try:
            ble.gatts_notify(conn_handle, char_handle, message.encode())
            last_send_time = current_time
        except Exception as e:
            print(f"메시지 전송 실패: {e}")
    
    time.sleep(0.1)  # CPU 부하 감소 