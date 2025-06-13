import network
import socket
import time
from machine import Pin

# 시작 시 1초 딜레이
print("트랜스미터 시작 대기 중...")
time.sleep(1)
print("트랜스미터 시작!")

# WiFi AP 설정
AP_SSID = "ESP32_AP"
AP_PASSWORD = "12345678"
PORT = 8080

# WiFi AP 모드 설정
ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid=AP_SSID, password=AP_PASSWORD, authmode=network.AUTH_WPA_WPA2_PSK)

# 전역 변수
server_sock = None
client_sock = None
connected = False
count = 0
last_client_time = 0
last_send_time = 0  # 마지막 전송 시간 추가

# 상수
SERVER_PORT = 8080
CLIENT_TIMEOUT = 10000  # 10초
SEND_INTERVAL = 5000    # 5초

def start_server():
    """서버 시작 (최초 1회만 실행)"""
    global server_sock
    
    try:
        # 서버 소켓 생성
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind(('0.0.0.0', PORT))
        server_sock.listen(1)
        server_sock.settimeout(5)  # 5초 타임아웃으로 수정
        
        print(f"서버 시작됨: {ap.ifconfig()[0]}:{PORT}")
        print("클라이언트 연결 대기 중...")
        return True
        
    except Exception as e:
        print(f"서버 시작 실패: {e}")
        return False

def cleanup_client():
    """클라이언트 연결만 정리"""
    global connected, client_sock, last_client_time
    
    if client_sock:
        try:
            client_sock.close()
        except:
            pass
        client_sock = None
    
    connected = False
    last_client_time = 0

def accept_client():
    """클라이언트 연결 수락"""
    global connected, client_sock, last_client_time
    
    try:
        if server_sock:
            client_sock, addr = server_sock.accept()
            client_sock.settimeout(5)  # 5초 타임아웃
            connected = True
            last_client_time = time.ticks_ms()
            print(f"클라이언트 연결됨: {addr[0]}:{addr[1]}")
            return True
    except OSError as e:
        if "timed out" in str(e):  # 타임아웃은 일반적인 상황이므로 메시지 출력하지 않음
            pass
        else:
            print(f"클라이언트 연결 실패: {e}")
            cleanup_client()
    except Exception as e:
        print(f"클라이언트 연결 실패: {e}")
        cleanup_client()
    return False

def check_client_timeout():
    """클라이언트 타임아웃 확인"""
    global connected, last_client_time
    
    if connected and time.ticks_diff(time.ticks_ms(), last_client_time) > CLIENT_TIMEOUT:
        print("클라이언트 타임아웃")
        cleanup_client()
        return True
    return False

def send_message(message):
    """메시지 전송"""
    global connected, client_sock, count, last_client_time, last_send_time
    
    try:
        if connected and client_sock:
            count += 1  # 메시지 전송 전에 카운트 증가
            # 메시지 전송
            client_sock.send(message.encode())
            print(f"[반복:{count} | 진행:1/4] 전송할 메시지 : {message}")
            last_client_time = time.ticks_ms()  # 전송 시간 갱신
            last_send_time = time.ticks_ms()    # 마지막 전송 시간 갱신
            return True
    except Exception as e:
        print(f"메시지 전송 실패: {e}")
        cleanup_client()
    return False

def receive_message():
    """메시지 수신"""
    global connected, client_sock, count, last_client_time
    
    try:
        if connected and client_sock:
            # 메시지 수신
            data = client_sock.recv(1024)
            if data:
                last_client_time = time.ticks_ms()  # 통신 시간 갱신
                message = data.decode()
                print(f"[반복:{count} | 진행:4/4] 수신한 메시지 : {message}")  # count 증가 제거
                return True
    except OSError as e:
        if "timed out" in str(e):  # 타임아웃은 일반적인 상황
            pass
        else:
            print(f"메시지 수신 실패: {e}")
            cleanup_client()
    except Exception as e:
        print(f"메시지 수신 실패: {e}")
        cleanup_client()
    return False

print("WiFi AP 시작...")
print(f"AP 정보: SSID={AP_SSID}, Password={AP_PASSWORD}")

# 서버 시작 (최초 1회만 실행)
if not start_server():
    print("서버 시작 실패, 재시작 중...")
    time.sleep(5)
    import machine
    machine.reset()

# 메인 루프
while True:
    try:
        # 클라이언트 타임아웃 확인
        if check_client_timeout():
            print("클라이언트 연결이 끊어짐. 새로운 연결 대기 중...")
            continue
            
        # 클라이언트 연결 대기
        if not connected:
            accept_client()  # 연결 요청이 있을 때만 처리
        else:
            # 5초마다 메시지 전송
            current_time = time.ticks_ms()
            if time.ticks_diff(current_time, last_send_time) > SEND_INTERVAL:
                if send_message("hello"):
                    last_send_time = current_time
                    # 응답 대기
                    receive_message()
        
        time.sleep(0.1)  # CPU 부하 감소
        
    except Exception as e:
        print(f"예기치 않은 오류 발생: {e}")
        cleanup_client()
        time.sleep(1) 