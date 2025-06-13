#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

// WiFi 설정
const char* ssid = "ESP32_Transmitter";  // 트랜스미터의 WiFi 네트워크 이름
const char* password = "12345678";       // WiFi 비밀번호
const int port = 8080;                   // 통신 포트

// MAC 주소 설정
const char* receiver_mac = "dc:06:75:67:f5:ee";  // 리시버의 MAC 주소

// 서버 객체 생성
WiFiServer server(port);
WiFiClient client;

// 메시지 카운터
uint32_t messageCounter = 0;

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  delay(100);  // 시리얼 통신 안정화를 위한 딜레이

  // WiFi SoftAP 모드 설정
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  // 서버 시작
  server.begin();
  
  Serial.println("트랜스미터 초기화 완료");
  Serial.printf("WiFi 네트워크 생성됨: %s\n", ssid);
  Serial.printf("IP 주소: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("포트: %d\n", port);
}

void loop() {
  // 클라이언트 연결 확인
  if (!client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("리시버 연결됨");
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
    client.println(message);
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