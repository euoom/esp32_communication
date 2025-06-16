#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

// WiFi 설정
const char* ssid = "ESP32_Transmitter";  // 트랜스미터의 WiFi 네트워크 이름
const char* password = "12345678";       // WiFi 비밀번호
const int port = 8080;                   // 통신 포트

// IP 설정
IPAddress staticIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// MAC 주소 설정
const char* receiver_mac = "dc:06:75:67:f5:ee";  // 리시버의 MAC 주소

// 서버 객체 생성
WiFiServer server(port);
WiFiClient client;

// 메시지 카운터
uint32_t messageCounter = 0;

// 연결 상태 관리
bool wasConnected = false;
unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 1000; // 1초마다 연결 상태 확인

// 연결 해제 처리 함수
void handleDisconnection() {
    if (client.connected()) {
        client.stop();
        Serial.println("연결이 정상적으로 종료되었습니다.");
    }
    wasConnected = false;
}

// MAC 주소 검증 함수
bool validateClientMac() {
    if (!client.connected()) return false;
    
    uint8_t clientMac[6];
    client.getMacAddress(clientMac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             clientMac[0], clientMac[1], clientMac[2],
             clientMac[3], clientMac[4], clientMac[5]);
             
    if (strcmp(macStr, receiver_mac) != 0) {
        Serial.printf("허용되지 않은 MAC 주소: %s\n", macStr);
        client.stop();
        return false;
    }
    return true;
}

void setup() {
    // 시리얼 통신 초기화
    Serial.begin(115200);
    delay(100);  // 시리얼 통신 안정화를 위한 딜레이

    // WiFi SoftAP 모드 설정
    WiFi.mode(WIFI_AP);
    
    // 고정 IP 설정
    if (!WiFi.softAPConfig(staticIP, gateway, subnet)) {
        Serial.println("AP IP 설정 실패");
        ESP.restart();
    }
    
    // AP 시작
    if (!WiFi.softAP(ssid, password)) {
        Serial.println("AP 시작 실패");
        ESP.restart();
    }
    
    // 서버 시작
    server.begin();
    
    Serial.println("트랜스미터 초기화 완료");
    Serial.printf("WiFi 네트워크 생성됨: %s\n", ssid);
    Serial.printf("IP 주소: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("포트: %d\n", port);
    Serial.printf("허용된 MAC 주소: %s\n", receiver_mac);
}

void loop() {
    // 주기적인 연결 상태 확인
    if (millis() - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
        lastConnectionCheck = millis();
        
        if (client.connected()) {
            if (!validateClientMac()) {
                handleDisconnection();
            }
        } else if (wasConnected) {
            handleDisconnection();
        }
    }

    // 클라이언트 연결 확인
    if (!client.connected()) {
        client = server.available();
        if (client) {
            if (validateClientMac()) {
                Serial.println("리시버 연결됨");
                wasConnected = true;
            }
        }
    }

    // 연결된 클라이언트가 있고 5초가 지났다면 메시지 전송
    static unsigned long lastSendTime = 0;
    if (client.connected() && (millis() - lastSendTime >= 5000)) {
        // 메시지 전송 준비
        String message = "hello";
        messageCounter++;
        
        // 메시지 전송 전 출력
        Serial.printf("[반복:%d | 진행:1/4] 전송할 메시지 : %s\n", messageCounter, message.c_str());
        
        // 메시지 전송
        if (!client.println(message)) {
            Serial.println("메시지 전송 실패");
            handleDisconnection();
            return;
        }
        lastSendTime = millis();
        
        // 응답 대기
        unsigned long timeout = millis() + 1000;  // 1초 타임아웃
        while (millis() < timeout) {
            if (client.available()) {
                String response = client.readStringUntil('\n');
                response.trim();
                
                // 응답 메시지 출력
                Serial.printf("[반복:%d | 진행:4/4] 수신한 메시지 : %s\n", messageCounter, response.c_str());
                break;
            }
            delay(10);
        }
    }
    
    delay(500);  // CPU 부하 감소
} 