#include <WiFi.h>

// WiFi AP 설정
const char* AP_SSID = "ESP32_LED_AP";
const char* AP_PASSWORD = "12345678";
const int PORT = 8080;

// ADC 설정
const int ADC_PIN = 3;  // GPIO 3번 핀
const int ADC_RESOLUTION = 12;  // 12비트 해상도
const int ADC_MAX = 4095;  // 12비트 최대값

// 서버 설정
WiFiServer server(PORT);
WiFiClient client;

// 전역 변수
bool connected = false;
unsigned long lastSendTime = 0;
unsigned long lastClientTime = 0;
int count = 0;

// 상수
const unsigned long CLIENT_TIMEOUT = 10000;  // 10초
const unsigned long SEND_INTERVAL = 5000;    // 5초

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  delay(100);  // 시리얼 통신 안정화 대기
  
  Serial.println("\n트랜스미터 시작 대기 중...");
  delay(1000);
  Serial.println("트랜스미터 시작!");

  // ADC 설정
  analogReadResolution(ADC_RESOLUTION);
  analogSetAttenuation(ADC_11db);  // 0-3.3V 범위

  // WiFi AP 모드 설정
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  // AP 정보 출력
  Serial.println("WiFi AP 시작...");
  Serial.printf("AP 정보: SSID=%s, Password=%s\n", AP_SSID, AP_PASSWORD);
  Serial.printf("AP IP 주소: %s\n", WiFi.softAPIP().toString().c_str());

  // 서버 시작
  server.begin();
  Serial.printf("서버 시작됨: %s:%d\n", WiFi.softAPIP().toString().c_str(), PORT);
  Serial.println("클라이언트 연결 대기 중...");
}

void cleanupClient() {
  if (client) {
    client.stop();
  }
  connected = false;
  lastClientTime = 0;
}

void checkClientTimeout() {
  if (connected && (millis() - lastClientTime > CLIENT_TIMEOUT)) {
    Serial.println("클라이언트 타임아웃");
    cleanupClient();
  }
}

int readPotentiometer() {
  return analogRead(ADC_PIN);
}

bool sendValue(int value) {
  if (connected && client.connected()) {
    count++;
    float voltage = (value * 3.3f) / ADC_MAX;
    Serial.printf("[반복:%d] 전송: ADC=%4d (전압: %.2fV)\n", count, value, voltage);
    
    // 값 전송
    client.println(value);
    lastClientTime = millis();
    return true;
  }
  return false;
}

void loop() {
  // 클라이언트 타임아웃 확인
  checkClientTimeout();

  // 새로운 클라이언트 연결 확인
  if (!connected) {
    client = server.available();
    if (client) {
      Serial.printf("클라이언트 연결됨: %s:%d\n", 
        client.remoteIP().toString().c_str(), client.remotePort());
      connected = true;
      lastClientTime = millis();
    }
  } else {
    // 연결된 클라이언트가 있는 경우
    if (!client.connected()) {
      Serial.println("클라이언트 연결 해제");
      cleanupClient();
    } else {
      // 5초마다 가변저항 값 읽고 전송
      if (millis() - lastSendTime > SEND_INTERVAL) {
        int currentValue = readPotentiometer();
        if (sendValue(currentValue)) {
          lastSendTime = millis();
        }
      }
    }
  }

  delay(100);  // CPU 부하 감소
} 