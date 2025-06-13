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

// 블루투스 클라이언트
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool connected = false;
unsigned long lastSendTime = 0;
int count = 0;
bool scanning = false;

// 연결 콜백
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("서버 연결됨!");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("서버 연결 끊김!");
    pRemoteCharacteristic = nullptr;  // 특성 초기화
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

  // 블루투스 초기화
  BLEDevice::init("ESP32_LED_TRANSMITTER");
  Serial.println("블루투스 클라이언트 초기화 완료");
  
  // MAC 주소로 직접 연결
  BLEAddress receiverAddress(RECEIVER_MAC);
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  Serial.printf("리시버 연결 시도... (MAC: %s)\n", RECEIVER_MAC);
  if (pClient->connect(receiverAddress)) {
    Serial.println("리시버 연결 성공!");
  } else {
    Serial.println("리시버 연결 실패!");
  }
}

void loop() {
  if (!connected) {
    // 연결이 끊어지면 재연결 시도
    Serial.println("서버 재연결 시도...");
    BLEAddress receiverAddress(RECEIVER_MAC);
    if (pClient->connect(receiverAddress)) {
      Serial.println("리시버 재연결 성공!");
    } else {
      Serial.println("리시버 재연결 실패!");
    }
    delay(1000);  // 재연결 시도 간격
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
      }
    } else {
      Serial.println("서비스를 찾을 수 없음!");
    }
    return;
  }

  // 5초마다 ADC 값을 읽어서 전송
  if (millis() - lastSendTime >= 5000) {
    int value = analogRead(LED_PIN);
    float voltage = (value * 3.3f) / ADC_MAX;
    
    // ADC 값을 문자열로 변환하여 전송
    String data = String(value);
    try {
      pRemoteCharacteristic->writeValue(data.c_str(), data.length());
      count++;
      Serial.printf("[반복:%d] 전송: ADC=%4d (전압: %.2fV)\n", 
        count, value, voltage);
    } catch (...) {
      Serial.println("데이터 전송 실패!");
    }
    
    lastSendTime = millis();
  }
} 