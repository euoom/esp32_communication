import bluetooth
from micropython import const
import struct
import time
from machine import Pin, reset

# 시작 시 1초 딜레이
print("리시버 시작 대기 중...")
time.sleep(1)
print("리시버 시작!")

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

# 트랜스미터 MAC 주소 (고정)
TRANSMITTER_MAC = bytes.fromhex("dc0675680b52")  # "dc:06:75:68:0b:52"에서 콜론 제거

# BLE 서비스와 특성 UUID (README.md와 일치)
SERVICE_UUID = bluetooth.UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
CHARACTERISTIC_UUID = bluetooth.UUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
CHARACTERISTIC_UUID_NOTIFY = bluetooth.UUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

# BLE 클라이언트 설정
ble = bluetooth.BLE()
ble.active(True)

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

# 재시도 제한
MAX_RETRY_COUNT = 5
retry_count = 0
last_retry_time = 0
RETRY_INTERVAL = 5000  # 재시도 간격 (ms)

def decode_message(data):
    """수신된 데이터를 문자열로 디코딩"""
    try:
        if isinstance(data, memoryview):
            return bytes(data).decode()
        elif isinstance(data, bytes):
            return data.decode()
        else:
            return str(data)
    except Exception as e:
        print(f"메시지 디코딩 실패: {e}")
        return ""

def start_scan():
    global scanning, last_scan_time, retry_count
    current_time = time.ticks_ms()
    
    # 재시도 횟수 확인
    if retry_count >= MAX_RETRY_COUNT:
        print("최대 재시도 횟수 초과. 재시작합니다.")
        reset()
        return False
    
    # 이전 스캔으로부터 충분한 시간이 지났는지 확인
    if not scanning and time.ticks_diff(current_time, last_scan_time) > SCAN_INTERVAL:
        try:
            # 이전 스캔이 있다면 먼저 중지
            ble.gap_scan(None)
            time.sleep(0.05)  # 스캔 중지 대기 시간 줄임
            
            # 새 스캔 시작
            ble.gap_scan(SCAN_DURATION, SCAN_INTERVAL, SCAN_WINDOW)
            scanning = True
            last_scan_time = current_time
            retry_count += 1
            print(f"스캔 시작... (시도 {retry_count}/{MAX_RETRY_COUNT})")
        except Exception as e:
            print(f"스캔 시작 실패: {e}")
            scanning = False
            time.sleep(0.1)
            return False
    else:
        if not scanning:
            print("스캔 대기 중...")
    return True

def stop_scan():
    global scanning
    if scanning:
        try:
            ble.gap_scan(None)
            scanning = False
            print("스캔 중지...")
            time.sleep(0.05)  # 스캔 중지 대기 시간 줄임
        except Exception as e:
            print(f"스캔 중지 실패: {e}")

def bt_irq(event, data):
    global connected, conn_handle, char_handle, count, scanning, retry_count
    try:
        if event == _IRQ_SCAN_RESULT:
            # 스캔 결과 처리
            addr_type, addr, adv_type, rssi, adv_data = data
            addr = bytes(addr)
            
            # 트랜스미터 MAC 주소와 일치하는지 확인
            if addr == TRANSMITTER_MAC:
                print(f"트랜스미터 발견! RSSI: {rssi}")
                # 스캔 중지 후 연결 시도
                stop_scan()
                time.sleep(0.05)  # 연결 전 대기 시간 줄임
                ble.gap_connect(addr_type, addr)
                
        elif event == _IRQ_SCAN_DONE:
            # 스캔 완료 후 재시작 (연결되지 않은 경우에만)
            scanning = False
            if not connected:
                print("스캔 완료, 재시작...")
                time.sleep(0.05)  # 재스캔 전 대기 시간 줄임
                start_scan()
            else:
                print("스캔 완료 (연결 유지 중)")
                
        elif event == _IRQ_PERIPHERAL_CONNECT:
            # 연결 성공
            conn_handle, addr_type, addr = data
            connected = True
            conn_handle = conn_handle
            retry_count = 0  # 연결 성공 시 재시도 카운트 초기화
            print(f"트랜스미터 연결됨: {bytes(addr).hex()}")
            # 서비스 검색 시작
            ble.gattc_discover_services(conn_handle)
            
        elif event == _IRQ_PERIPHERAL_DISCONNECT:
            # 연결 해제
            conn_handle, addr_type, addr = data
            connected = False
            conn_handle = None
            char_handle = None
            print("트랜스미터 연결 해제, 재스캔 시작...")
            # 재시도 횟수 증가
            retry_count += 1
            if retry_count >= MAX_RETRY_COUNT:
                print("최대 재시도 횟수 초과. 재시작합니다.")
                reset()
            # 재스캔 시작
            time.sleep(0.2)  # 연결 해제 후 대기 시간 조정
            start_scan()
            
        elif event == _IRQ_GATTC_SERVICE_RESULT:
            # 서비스 검색 결과
            conn_handle, start_handle, end_handle, uuid = data
            if uuid == SERVICE_UUID:
                print("서비스 발견!")
                # 특성 검색 시작
                ble.gattc_discover_characteristics(conn_handle, start_handle, end_handle)
                
        elif event == _IRQ_GATTC_CHARACTERISTIC_RESULT:
            # 특성 검색 결과
            conn_handle, def_handle, value_handle, properties, uuid = data
            if uuid == CHARACTERISTIC_UUID:
                print("특성 발견!")
                char_handle = value_handle
                # 알림 활성화
                ble.gattc_write(conn_handle, value_handle + 1, struct.pack("<h", 0x0001))
                
        elif event == _IRQ_GATTC_NOTIFY:
            # 알림 수신
            conn_handle, value_handle, notify_data = data
            count += 1
            
            # 수신된 데이터 디코딩
            message = decode_message(notify_data)
            if message:
                print(f"[반복:{count} | 진행:2/4] 수신한 메시지 : {message}")
                
                # 응답 메시지 생성 및 전송
                response = f"{message} world"
                print(f"[반복:{count} | 진행:3/4] 응답할 메시지 : {response}")
                
                if connected and char_handle is not None:
                    try:
                        ble.gattc_write(conn_handle, char_handle, response.encode())
                    except Exception as e:
                        print(f"응답 전송 실패: {e}")
                        retry_count += 1
                        if retry_count >= MAX_RETRY_COUNT:
                            print("최대 재시도 횟수 초과. 재시작합니다.")
                            reset()
            else:
                print(f"[반복:{count}] 수신된 메시지 디코딩 실패")
                
    except Exception as e:
        print(f"이벤트 처리 중 오류 발생: {e}")
        # 오류 발생 시 스캔 상태 초기화
        scanning = False
        retry_count += 1
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
        time.sleep(0.2)  # 오류 후 대기 시간 조정
        if not connected:
            start_scan()

# BLE 콜백 설정
ble.irq(bt_irq)

print("블루투스 클라이언트 시작...")
print(f"트랜스미터({TRANSMITTER_MAC.hex()}) 스캔 중...")
print(f"서비스 UUID: {SERVICE_UUID}")
print(f"특성 UUID: {CHARACTERISTIC_UUID}")
print(f"알림 특성 UUID: {CHARACTERISTIC_UUID_NOTIFY}")

# 초기 스캔 시작
time.sleep(0.2)  # 시작 전 대기 시간 조정
start_scan()

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    # 재시도 간격 확인
    if not connected and time.ticks_diff(current_time, last_retry_time) > RETRY_INTERVAL:
        last_retry_time = current_time
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
    
    if not connected and not scanning:
        start_scan()  # 연결되지 않은 상태에서 스캔이 중지되었다면 재시작
    time.sleep(0.1) 