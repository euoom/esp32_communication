#include <WiFi.h>
#include <WiFiClient.h>

// WiFi 설정
const char* ssid = "ESP32_Transmitter";  // 트랜스미터의 WiFi 네트워크 이름
const char* password = "12345678";       // WiFi 비밀번호
const int port = 8080;                   // 통신 포트

// MAC 주소 설정
const char* transmitter_mac = "dc:06:75:68:0b:52";  // 트랜스미터의 MAC 주소

// 클라이언트 객체 생성
WiFiClient client;

// 메시지 카운터
uint32_t messageCounter = 0;

// WiFi 연결 함수
bool connectToTransmitter() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("트랜스미터에 연결 시도 중...");
    WiFi.begin(ssid, password);
    
    // 연결 시도 (최대 10초)
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("트랜스미터 연결 성공");
      Serial.printf("IP 주소: %s\n", WiFi.localIP().toString().c_str());
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
  // WiFi 연결 상태 확인 및 재연결
  if (!connectToTransmitter()) {
    delay(1000);
    return;
  }

  // 서버 연결 확인
  if (!client.connected()) {
    if (client.connect(WiFi.gatewayIP(), port)) {
      Serial.println("서버 연결됨");
    } else {
      Serial.println("서버 연결 실패");
      delay(1000);
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
    client.println(responseMessage);
  }
  
  delay(500);  // CPU 부하 감소
} 