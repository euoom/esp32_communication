import network
import socket
import time
from machine import Pin, reset

# 시작 시 1초 딜레이
print("리시버 시작 대기 중...")
time.sleep(1)
print("리시버 시작!")

# WiFi 설정
WIFI_SSID = "ESP32_AP"  # 트랜스미터의 AP 이름
WIFI_PASSWORD = "12345678"  # 트랜스미터의 AP 비밀번호
PORT = 8080  # 통신 포트

# WiFi 스테이션 모드 설정
wlan = network.WLAN(network.STA_IF)
wlan.active(True)

# 연결 상태
connected = False
sock = None
count = 0
last_send_time = 0
last_retry_time = 0

# 상수
SEND_INTERVAL = 5000    # 메시지 전송 간격 (ms)
MAX_RETRY_COUNT = 5     # 최대 재시도 횟수
RETRY_INTERVAL = 5000   # 재시도 간격 (ms)
CONNECT_TIMEOUT = 30    # 연결 시도 타임아웃 (초)
retry_count = 0         # 현재 재시도 횟수

def connect_to_transmitter():
    """트랜스미터에 연결"""
    global connected, sock, retry_count
    
    try:
        # 이전 연결이 있다면 정리
        if sock:
            try:
                sock.close()
            except:
                pass
            sock = None
        connected = False
        
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
        max_wait = CONNECT_TIMEOUT
        while max_wait > 0:
            status = wlan.status()
            if status == network.STAT_GOT_IP:
                break
            elif status == network.STAT_CONNECTING:
                print(f"연결 중... {max_wait}초 남음")
            elif status == network.STAT_CONNECT_FAIL:
                print("연결 실패")
                retry_count += 1
                if retry_count >= MAX_RETRY_COUNT:
                    print("최대 재시도 횟수 초과. 재시작합니다.")
                    reset()
                return False
            elif status == network.STAT_NO_AP_FOUND:
                print("AP를 찾을 수 없음")
                retry_count += 1
                if retry_count >= MAX_RETRY_COUNT:
                    print("최대 재시도 횟수 초과. 재시작합니다.")
                    reset()
                return False
            max_wait -= 1
            time.sleep(1)
        
        if not wlan.isconnected():
            print("WiFi 연결 실패")
            retry_count += 1
            if retry_count >= MAX_RETRY_COUNT:
                print("최대 재시도 횟수 초과. 재시작합니다.")
                reset()
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
                retry_count = 0  # 연결 성공 시 재시도 카운트 초기화
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
        retry_count += 1
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
        return False
        
    except Exception as e:
        print(f"연결 중 예외 발생: {e}")
        if sock:
            try:
                sock.close()
            except:
                pass
            sock = None
        connected = False
        retry_count += 1
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
        return False

def send_message(message):
    """메시지 전송"""
    global sock, count, connected, retry_count
    
    try:
        if not connected or not sock:
            print("연결이 끊어짐")
            connected = False
            retry_count += 1
            if retry_count >= MAX_RETRY_COUNT:
                print("최대 재시도 횟수 초과. 재시작합니다.")
                reset()
            return False
            
        # 메시지 전송
        sock.send(message.encode())
        print(f"[반복:{count} | 진행:1/4] 전송할 메시지 : {message}")
        retry_count = 0  # 전송 성공 시 재시도 카운트 초기화
        return True
        
    except Exception as e:
        print(f"메시지 전송 실패: {e}")
        connected = False
        if sock:
            try:
                sock.close()
            except:
                pass
            sock = None
        retry_count += 1
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
        return False

def receive_message():
    """메시지 수신"""
    global sock, count, connected, retry_count
    
    try:
        if not connected or not sock:
            print("연결이 끊어짐")
            connected = False
            retry_count += 1
            if retry_count >= MAX_RETRY_COUNT:
                print("최대 재시도 횟수 초과. 재시작합니다.")
                reset()
            return False
            
        # 메시지 수신
        data = sock.recv(1024)
        if data:
            count += 1  # 메시지 수신 시 카운트 증가
            message = data.decode()
            print(f"[반복:{count} | 진행:2/4] 수신한 메시지 : {message}")
            
            # 응답 메시지 생성 및 전송
            response = f"{message} world"
            print(f"[반복:{count} | 진행:3/4] 응답할 메시지 : {response}")
            
            try:
                sock.send(response.encode())
                retry_count = 0  # 수신 및 응답 성공 시 재시도 카운트 초기화
                return True
            except Exception as e:
                print(f"응답 전송 실패: {e}")
                connected = False
                if sock:
                    try:
                        sock.close()
                    except:
                        pass
                    sock = None
                retry_count += 1
                if retry_count >= MAX_RETRY_COUNT:
                    print("최대 재시도 횟수 초과. 재시작합니다.")
                    reset()
                return False
            
    except OSError as e:
        if "timed out" in str(e):  # 타임아웃은 일반적인 상황
            pass
        else:
            print(f"메시지 수신 실패: {e}")
            connected = False
            if sock:
                try:
                    sock.close()
                except:
                    pass
                sock = None
            retry_count += 1
            if retry_count >= MAX_RETRY_COUNT:
                print("최대 재시도 횟수 초과. 재시작합니다.")
                reset()
    except Exception as e:
        print(f"메시지 수신 실패: {e}")
        connected = False
        if sock:
            try:
                sock.close()
            except:
                pass
            sock = None
        retry_count += 1
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
    return False

print("WiFi 클라이언트 시작...")
print(f"AP 정보: SSID={WIFI_SSID}, Password={WIFI_PASSWORD}")
print(f"서버 포트: {PORT}")
print(f"최대 재시도 횟수: {MAX_RETRY_COUNT}")
print(f"재시도 간격: {RETRY_INTERVAL}ms")
print(f"연결 타임아웃: {CONNECT_TIMEOUT}초")

# 메인 루프
while True:
    current_time = time.ticks_ms()
    
    # 재시도 간격 확인
    if not connected and time.ticks_diff(current_time, last_retry_time) > RETRY_INTERVAL:
        last_retry_time = current_time
        if retry_count >= MAX_RETRY_COUNT:
            print("최대 재시도 횟수 초과. 재시작합니다.")
            reset()
    
    # 연결이 끊어진 경우 재연결
    if not connected:
        if connect_to_transmitter():
            print("연결 성공, 2초 대기 후 메시지 수신 시작")
            time.sleep(2)  # 연결 후 대기 시간
        else:
            print("재연결 시도 중... 10초 후 다시 시도합니다.")
            time.sleep(10)  # 재연결 실패 시 대기 시간
        continue
    
    # 메시지 수신 및 응답
    receive_message()
    
    time.sleep(0.1)  # CPU 부하 감소 