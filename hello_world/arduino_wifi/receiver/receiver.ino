#include <WiFi.h>
#include <WiFiClient.h>

// WiFi 설정
const char* ssid = "ESP32_Transmitter";  // 트랜스미터의 WiFi 네트워크 이름
const char* password = "12345678";       // WiFi 비밀번호
const int port = 8080;                   // 통신 포트

// IP 설정
IPAddress staticIP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// MAC 주소 설정
const char* transmitter_mac = "dc:06:75:68:0b:52";  // 트랜스미터의 MAC 주소

// 클라이언트 객체 생성
WiFiClient client;

// 메시지 카운터
uint32_t messageCounter = 0;

// 연결 상태 관리
bool wasConnected = false;
unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 1000; // 1초마다 연결 상태 확인
const unsigned long RECONNECT_DELAY = 5000; // 재연결 시도 간격 (5초)
unsigned long lastReconnectAttempt = 0;

// 연결 해제 처리 함수
void handleDisconnection() {
    if (client.connected()) {
        client.stop();
        Serial.println("연결이 정상적으로 종료되었습니다.");
    }
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        Serial.println("WiFi 연결 해제");
    }
    wasConnected = false;
}

// WiFi 연결 함수
bool connectToTransmitter() {
    if (WiFi.status() != WL_CONNECTED) {
        // 재연결 시도 간격 확인
        if (millis() - lastReconnectAttempt < RECONNECT_DELAY) {
            return false;
        }
        lastReconnectAttempt = millis();

        Serial.println("트랜스미터에 연결 시도 중...");
        
        // 고정 IP 설정
        WiFi.config(staticIP, gateway, subnet);
        WiFi.begin(ssid, password);
        
        // 연결 시도 (최대 10초)
        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            // 트랜스미터의 MAC 주소 확인
            uint8_t apMac[6];
            WiFi.BSSID(apMac);
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                     apMac[0], apMac[1], apMac[2],
                     apMac[3], apMac[4], apMac[5]);
                     
            if (strcmp(macStr, transmitter_mac) != 0) {
                Serial.printf("허용되지 않은 AP MAC 주소: %s\n", macStr);
                WiFi.disconnect();
                return false;
            }
            
            Serial.println("트랜스미터 연결 성공");
            Serial.printf("IP 주소: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("AP MAC 주소: %s\n", macStr);
            return true;
        } else {
            Serial.println("트랜스미터 연결 실패");
            return false;
        }
    }
    return true;
}

void setup() {
    // 시리얼 통신 초기화
    Serial.begin(115200);
    delay(100);  // 시리얼 통신 안정화를 위한 딜레이

    // WiFi Station 모드 설정
    WiFi.mode(WIFI_STA);
    
    // 트랜스미터 연결
    connectToTransmitter();
}

void loop() {
    // 주기적인 연결 상태 확인
    if (millis() - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
        lastConnectionCheck = millis();
        
        if (!connectToTransmitter()) {
            if (wasConnected) {
                handleDisconnection();
            }
            return;
        }
    }

    // 서버 연결 확인
    if (!client.connected()) {
        if (client.connect(WiFi.gatewayIP(), port)) {
            Serial.println("서버 연결됨");
            wasConnected = true;
        } else {
            if (wasConnected) {
                handleDisconnection();
            }
            return;
        }
    }

    // 메시지 수신 확인
    if (client.available()) {
        String message = client.readStringUntil('\n');
        message.trim();
        messageCounter++;
        
        // 수신한 메시지 출력
        Serial.printf("[반복:%d | 진행:2/4] 수신한 메시지 : %s\n", messageCounter, message.c_str());
        
        // 응답 메시지 생성 및 전송
        String responseMessage = message + " world";
        Serial.printf("[반복:%d | 진행:3/4] 응답할 메시지 : %s\n", messageCounter, responseMessage.c_str());
        
        if (!client.println(responseMessage)) {
            Serial.println("응답 전송 실패");
            handleDisconnection();
            return;
        }
    }
    
    delay(500);  // CPU 부하 감소
} 