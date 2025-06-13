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
const int PWM_FREQ = 1000;  // PWM 주파수 1kHz
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_6_BIT;  // 6비트 해상도 (ESP32-C3 제한)
const int PWM_MAX = 63;  // 6비트 최대값 (2^6 - 1)

// 블루투스 서버
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool connected = false;
int count = 0;

// 연결 콜백
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    connected = true;
    Serial.println("클라이언트 연결됨!");
  }

  void onDisconnect(BLEServer* pServer) {
    connected = false;
    Serial.println("클라이언트 연결 끊김!");
    // 연결이 끊어지면 LED 끄기
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
  }
};

// 특성 콜백
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();  // std::string을 c_str()로 변환 후 String으로
    if (rxValue.length() > 0) {
      try {
        count++;
        int adcValue = rxValue.toInt();
        float voltage = (adcValue * 3.3f) / 4095;
        Serial.printf("[반복:%d] 수신: ADC=%4d (전압: %.2fV)\n", 
          count, adcValue, voltage);
        
        // LED 제어 (ADC 값을 PWM 듀티로 변환, 6비트로 매핑)
        int duty = map(adcValue, 0, 4095, 0, PWM_MAX);  // 12비트 ADC를 6비트 PWM으로 변환
        ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
        ledc_update_duty(PWM_MODE, PWM_CHANNEL);
      } catch (...) {
        Serial.printf("잘못된 데이터 수신: %s\n", rxValue.c_str());
      }
    }
  }
};

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
}

void loop() {
  // 블루투스 연결 상태에 따라 LED 제어
  if (!connected) {
    // 연결이 끊어지면 LED 끄기
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
  }
  delay(1000);  // CPU 부하 감소
} 