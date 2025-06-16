#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <driver/ledc.h>

// 블루투스 설정
const char* DEVICE_NAME = "ESP32_LED_RECEIVER";
const char* SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const char* CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

// LED 설정 (PWM)
const int LED_PIN = 3;  // GPIO 3번 핀 (led_control_rules에 정의된 대로)
const ledc_mode_t PWM_MODE = LEDC_LOW_SPEED_MODE;  // 저속 모드
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_3;  // PWM 채널 3
const ledc_timer_t PWM_TIMER = LEDC_TIMER_3;  // 타이머 3
const int PWM_FREQ = 5000;  // PWM 주파수 5kHz
const ledc_timer_bit_t PWM_RESOLUTION = 8;  // 8비트 해상도
const int PWM_MAX = 255;  // 8비트 최대값 (2^8 - 1)
const int PWM_MIN = 0;     // PWM 최소값
const int PWM_THRESHOLD = 5;  // 노이즈 제거 임계값

// ADC-PWM 변환 설정
const int ADC_MAX = 4095;  // 12비트 ADC 최대값
const int ADC_MIN = 0;     // ADC 최소값

// 블루투스 서버
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool connected = false;
int count = 0;
int lastPwmValue = 0;  // 마지막 PWM 값 저장

// 연결 상태 모니터링 관련 상수
const int CONNECTION_TIMEOUT = 10000;  // 10초
const int MAX_RETRY_COUNT = 5;
const int RETRY_INTERVAL = 5000;  // 5초
int retry_count = 0;
unsigned long last_connection_time = 0;
bool is_advertising = false;

// 연결 콜백
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    connected = true;
    Serial.println("클라이언트 연결됨");
    retry_count = 0;
    last_connection_time = millis();
    if (is_advertising) {
      pServer->getAdvertising()->stop();
      is_advertising = false;
      Serial.println("광고 중지");
    }
  }

  void onDisconnect(BLEServer* pServer) {
    connected = false;
    Serial.println("클라이언트 연결 끊김");
    if (retry_count < MAX_RETRY_COUNT) {
      retry_count++;
      Serial.printf("재연결 광고 시작 (시도 %d/%d)\n", retry_count, MAX_RETRY_COUNT);
      pServer->getAdvertising()->start();
      is_advertising = true;
      last_connection_time = millis();
    } else {
      Serial.println("최대 재시도 횟수 초과. 장치를 재시작합니다.");
      ESP.restart();
    }
  }
};

// 특성 콜백
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();
    if (rxValue.length() > 0) {
      try {
        count++;
        int adcValue = rxValue.toInt();
        
        // ADC 값 범위 검증
        if (adcValue < ADC_MIN || adcValue > ADC_MAX) {
          Serial.printf("잘못된 ADC 값: %d (범위: %d-%d)\n", adcValue, ADC_MIN, ADC_MAX);
          return;
        }
        
        float voltage = (adcValue * 3.3f) / ADC_MAX;
        Serial.printf("[반복:%d] 수신: ADC=%4d (전압: %.2fV)\n", 
          count, adcValue, voltage);
        
        // LED 제어 (ADC 값을 PWM 듀티로 변환)
        int pwmValue = mapADCToPWM(adcValue);
        ledcWrite(PWM_CHANNEL, pwmValue);
        last_connection_time = millis();  // 데이터 수신 시 연결 시간 갱신
        
      } catch (...) {
        Serial.printf("잘못된 데이터 수신: %s\n", rxValue.c_str());
      }
    }
  }
};

// ADC 값을 PWM 값으로 변환 (노이즈 제거 포함)
int mapADCToPWM(int adc_value) {
  // ADC 값 범위 검증
  if (adc_value < 0) adc_value = 0;
  if (adc_value > 4095) adc_value = 4095;
  
  // ADC 값을 PWM 범위로 매핑
  int pwm_value = map(adc_value, 0, 4095, PWM_MIN, PWM_MAX);
  
  // 노이즈 제거 (임계값 이하의 변화는 무시)
  static int last_pwm = 0;
  if (abs(pwm_value - last_pwm) <= PWM_THRESHOLD) {
    return last_pwm;
  }
  last_pwm = pwm_value;
  return pwm_value;
}

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  delay(100);  // 시리얼 통신 안정화 대기
  
  Serial.println("\n리시버 시작 대기 중...");
  delay(1000);
  Serial.println("리시버 시작!");

  // LED PWM 설정
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
  ledcWrite(PWM_CHANNEL, 0);

  // 블루투스 초기화
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  
  Serial.println("블루투스 서버 시작!");
  Serial.printf("기기 이름: %s\n", DEVICE_NAME);
  Serial.printf("서비스 UUID: %s\n", SERVICE_UUID);
  Serial.printf("특성 UUID: %s\n", CHARACTERISTIC_UUID);
  Serial.printf("PWM 설정: %dHz, %d비트 해상도\n", PWM_FREQ, (int)PWM_RESOLUTION);
  Serial.printf("PWM 범위: %d-%d\n", PWM_MIN, PWM_MAX);
  Serial.printf("ADC 범위: %d-%d\n", ADC_MIN, ADC_MAX);
  Serial.printf("PWM 변화 임계값: %d\n", PWM_THRESHOLD);

  // 광고 시작
  pServer->getAdvertising()->start();
  is_advertising = true;
  Serial.println("광고 시작");
}

void loop() {
  // 연결 상태 모니터링
  if (pServer->getConnectedCount() > 0) {
    // 연결 타임아웃 체크
    if (millis() - last_connection_time > CONNECTION_TIMEOUT) {
      Serial.println("연결 타임아웃");
      pServer->disconnect(pServer->getPeerInfo(0).getConnId());
    }
  } else if (is_advertising) {
    // 재시도 간격 체크
    if (millis() - last_connection_time > RETRY_INTERVAL) {
      if (retry_count < MAX_RETRY_COUNT) {
        Serial.printf("재연결 광고 재시작 (시도 %d/%d)\n", retry_count, MAX_RETRY_COUNT);
        pServer->getAdvertising()->start();
        last_connection_time = millis();
      } else {
        Serial.println("최대 재시도 횟수 초과. 장치를 재시작합니다.");
        ESP.restart();
      }
    }
  }
  
  delay(100);  // CPU 부하 감소
} 