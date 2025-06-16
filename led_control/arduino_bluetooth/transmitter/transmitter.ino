#include <BLEDevice.h>
#include <BLEClient.h>

// 블루투스 설정
const char* DEVICE_NAME = "ESP32_LED_RECEIVER";  // 리시버의 블루투스 이름
const char* SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";  // 서비스 UUID
const char* CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // 송신용 특성 UUID
const char* RECEIVER_MAC = "dc:06:75:67:f5:ee";  // 리시버의 MAC 주소

// LED 설정 (ADC)
const int LED_PIN = 3;  // GPIO 3번 핀 (led_control_rules에 정의된 대로)
const int ADC_MAX = 4095;  // 12비트 ADC 최대값
const int ADC_SAMPLES = 5;  // ADC 샘플링 횟수
const int ADC_DELAY = 10;  // ADC 샘플링 간격 (ms)

// 블루투스 클라이언트
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool connected = false;
unsigned long lastSendTime = 0;
unsigned long lastRetryTime = 0;
int count = 0;
bool scanning = false;

// 재시도 제한
const int MAX_RETRY_COUNT = 5;
const int RETRY_INTERVAL = 5000;  // 5초
int retry_count = 0;
unsigned long last_retry_time = 0;

// ADC 필터링 관련 상수
const int ADC_PIN = 3;
int adc_values[ADC_SAMPLES];
int adc_index = 0;

// ADC 값 필터링 함수
int getFilteredADC() {
    // ADC 값 읽기
    adc_values[adc_index] = analogRead(ADC_PIN);
    adc_index = (adc_index + 1) % ADC_SAMPLES;
    
    // 이동 평균 계산
    long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += adc_values[i];
    }
    return sum / ADC_SAMPLES;
}

// 연결 상태 콜백 클래스
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("연결 성공");
        retry_count = 0;  // 연결 성공 시 재시도 카운트 초기화
    }

    void onDisconnect(BLEClient* pclient) {
        Serial.println("연결 끊김");
        if (retry_count < MAX_RETRY_COUNT) {
            retry_count++;
            last_retry_time = millis();
            Serial.printf("재연결 시도 %d/%d\n", retry_count, MAX_RETRY_COUNT);
        } else {
            Serial.println("최대 재시도 횟수 초과. 장치를 재시작합니다.");
            ESP.restart();
        }
    }
};

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  delay(100);  // 시리얼 통신 안정화 대기
  
  Serial.println("\n트랜스미터 시작 대기 중...");
  delay(1000);
  Serial.println("트랜스미터 시작!");

  // ADC 설정
  pinMode(LED_PIN, INPUT);  // ADC 핀 설정
  analogSetWidth(12);  // 12비트 ADC 설정
  analogSetAttenuation(ADC_11db);  // 3.3V 기준 전압 설정

  // ADC 버퍼 초기화
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adc_values[i] = 0;
  }

  // 블루투스 초기화
  BLEDevice::init("ESP32_LED_TRANSMITTER");
  Serial.println("블루투스 클라이언트 초기화 완료");
  
  // MAC 주소로 직접 연결
  BLEAddress receiverAddress(RECEIVER_MAC);
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  Serial.printf("리시버 연결 시도... (MAC: %s)\n", RECEIVER_MAC);
  Serial.printf("최대 재시도 횟수: %d\n", MAX_RETRY_COUNT);
  Serial.printf("재시도 간격: %dms\n", RETRY_INTERVAL);
  
  if (pClient->connect(receiverAddress)) {
    Serial.println("리시버 연결 성공!");
  } else {
    Serial.println("리시버 연결 실패!");
    retry_count++;
  }
}

void loop() {
  // 재시도 간격 확인
  if (retry_count > 0 && millis() - last_retry_time < RETRY_INTERVAL) {
    delay(100);
    return;
  }
  
  // 연결 상태 확인
  if (!pClient->isConnected()) {
    if (retry_count < MAX_RETRY_COUNT) {
      Serial.println("연결 끊김. 재연결 시도...");
      if (pClient->connect(BLEAddress(RECEIVER_MAC))) {
        Serial.println("재연결 성공");
        retry_count = 0;
      } else {
        retry_count++;
        last_retry_time = millis();
        Serial.printf("재연결 실패. 재시도 %d/%d\n", retry_count, MAX_RETRY_COUNT);
      }
    } else {
      Serial.println("최대 재시도 횟수 초과. 장치를 재시작합니다.");
      ESP.restart();
    }
    delay(1000);
    return;
  }

  // 서비스와 특성 찾기
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("서비스 검색 중...");
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
    if (pRemoteService != nullptr) {
      Serial.println("서비스 발견!");
      pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
      if (pRemoteCharacteristic != nullptr) {
        Serial.println("특성 발견! 데이터 전송 준비 완료");
      } else {
        Serial.println("특성을 찾을 수 없음!");
        retry_count++;
      }
    } else {
      Serial.println("서비스를 찾을 수 없음!");
      retry_count++;
    }
    return;
  }

  // ADC 값 읽기 및 필터링
  int adc_value = getFilteredADC();
  float voltage = (adc_value * 3.3f) / ADC_MAX;
  
  // ADC 값을 문자열로 변환하여 전송
  String data = String(adc_value);
  try {
    pRemoteCharacteristic->writeValue(data.c_str(), data.length());
    count++;
    Serial.printf("[반복:%d] 전송: ADC=%4d (전압: %.2fV)\n", 
      count, adc_value, voltage);
    retry_count = 0;  // 전송 성공 시 재시도 카운트 초기화
  } catch (...) {
    Serial.println("데이터 전송 실패!");
    retry_count++;
    if (retry_count >= MAX_RETRY_COUNT) {
      Serial.println("최대 재시도 횟수 초과. 재시작합니다.");
      ESP.restart();
    }
  }
  
  delay(5000);  // 5초 대기
} 