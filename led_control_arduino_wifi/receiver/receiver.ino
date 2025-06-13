#include <WiFi.h>
#include <driver/ledc.h>  // ESP32-C3 PWM 라이브러리

// WiFi 설정
const char* WIFI_SSID = "ESP32_LED_AP";  // 트랜스미터의 AP 이름
const char* WIFI_PASSWORD = "12345678";  // 트랜스미터의 AP 비밀번호
const int PORT = 8080;  // 통신 포트

// LED 설정 (PWM)
const int LED_PIN = 3;  // GPIO 3번 핀 (led_control_rules에 정의된 대로)
const ledc_mode_t PWM_MODE = LEDC_LOW_SPEED_MODE;  // 저속 모드
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_3;  // PWM 채널 3
const ledc_timer_t PWM_TIMER = LEDC_TIMER_3;  // 타이머 3
const int PWM_FREQ = 1000;  // PWM 주파수 1kHz
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_6_BIT;  // 6비트 해상도 (ESP32-C3 제한)
const int PWM_MAX = 63;  // 6비트 최대값 (2^6 - 1)

// WiFi 클라이언트
WiFiClient client;

// 연결 상태
bool connected = false;
unsigned long lastServerTime = 0;
int count = 0;

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  delay(100);  // 시리얼 통신 안정화 대기
  
  Serial.println("\n리시버 시작 대기 중...");
  delay(1000);
  Serial.println("리시버 시작!");

  // LED 설정
  ledc_timer_config_t timer_conf = {
    .speed_mode = PWM_MODE,
    .duty_resolution = PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = PWM_FREQ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  ledc_channel_config_t channel_conf = {
    .gpio_num = LED_PIN,
    .speed_mode = PWM_MODE,
    .channel = PWM_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = PWM_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);

  // LED 초기값 0 (꺼짐)
  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
  ledc_update_duty(PWM_MODE, PWM_CHANNEL);

  // WiFi 스테이션 모드 설정
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.println("WiFi 클라이언트 시작...");
  Serial.printf("AP 정보: SSID=%s, Password=%s\n", WIFI_SSID, WIFI_PASSWORD);
}

void cleanupConnection() {
  if (client) {
    client.stop();
  }
  connected = false;
  lastServerTime = 0;
  count = 0;
  // LED 끄기
  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
  ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

bool connectToTransmitter() {
  // 이전 연결이 있다면 정리
  cleanupConnection();
  
  // WiFi 연결 상태 확인
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("기존 WiFi 연결 해제 중...");
    WiFi.disconnect();
    delay(1000);
  }
  
  // 트랜스미터 AP에 연결
  Serial.println("트랜스미터 AP에 연결 중...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // 연결 대기 (최대 30초)
  int maxWait = 30;
  while (maxWait > 0) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    Serial.printf("연결 중... %d초 남음\n", maxWait);
    delay(1000);
    maxWait--;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 연결 실패");
    return false;
  }
  
  // IP 정보 출력
  Serial.printf("WiFi 연결됨: %s\n", WiFi.localIP().toString().c_str());
  
  // 서버 IP는 게이트웨이 IP (AP의 IP)
  IPAddress serverIP = WiFi.gatewayIP();
  Serial.printf("트랜스미터 IP: %s\n", serverIP.toString().c_str());
  
  // 서버에 연결 (최대 3번 시도)
  for (int attempt = 0; attempt < 3; attempt++) {
    Serial.printf("트랜스미터(%s:%d)에 연결 시도 중... (시도 %d/3)\n", 
      serverIP.toString().c_str(), PORT, attempt + 1);
    
    if (client.connect(serverIP, PORT)) {
      connected = true;
      lastServerTime = millis();
      Serial.printf("트랜스미터(%s:%d)에 연결됨\n", 
        serverIP.toString().c_str(), PORT);
      return true;
    }
    
    Serial.printf("연결 시도 %d 실패\n", attempt + 1);
    if (attempt < 2) {
      Serial.println("3초 후 재시도...");
      delay(3000);
    }
  }
  
  Serial.println("모든 연결 시도 실패");
  return false;
}

bool receiveAndControl() {
  if (!connected || !client.connected()) {
    Serial.println("연결이 끊어짐");
    cleanupConnection();
    return false;
  }
  
  // 값 수신
  if (client.available()) {
    String data = client.readStringUntil('\n');
    data.trim();
    
    if (data.length() > 0) {
      try {
        count++;
        int value = data.toInt();
        float voltage = (value * 3.3f) / 4095;
        Serial.printf("[반복:%d] 수신: ADC=%4d (전압: %.2fV)\n", 
          count, value, voltage);
        
        // LED 제어 (ADC 값을 PWM 듀티로 변환, 6비트로 매핑)
        int duty = map(value, 0, 4095, 0, PWM_MAX);  // 12비트 ADC를 6비트 PWM으로 변환
        ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
        ledc_update_duty(PWM_MODE, PWM_CHANNEL);
        
        lastServerTime = millis();
        return true;
      } catch (...) {
        Serial.printf("잘못된 데이터 수신: %s\n", data.c_str());
      }
    }
  }
  
  // 서버 타임아웃 확인 (10초)
  if (millis() - lastServerTime > 10000) {
    Serial.println("서버 타임아웃");
    cleanupConnection();
  }
  
  return false;
}

void loop() {
  // 연결이 끊어진 경우 재연결
  if (!connected) {
    if (connectToTransmitter()) {
      Serial.println("연결 성공, 2초 대기 후 데이터 수신 시작");
      delay(2000);  // 연결 후 대기 시간
    } else {
      Serial.println("재연결 시도 중... 10초 후 다시 시도합니다.");
      delay(10000);  // 재연결 실패 시 대기 시간
    }
    return;
  }
  
  // 값 수신 및 LED 제어
  receiveAndControl();
  
  delay(100);  // CPU 부하 감소
} 