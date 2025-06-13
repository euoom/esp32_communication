#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 서비스와 특성 UUID 정의 (트랜스미터와 동일)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE 서버 객체
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// 메시지 카운터 추가
uint32_t messageCounter = 0;

// 연결 상태 콜백 클래스
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("트랜스미터가 연결되었습니다!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("트랜스미터 연결이 끊어졌습니다.");
        // 재연결을 위해 광고 재시작
        pServer->getAdvertising()->start();
    }
};

// 특성 콜백 클래스
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            messageCounter++;  // 카운터 증가
            Serial.printf("[반복:%d | 진행:2/4] 수신한 메시지 : %s\n", messageCounter, rxValue.c_str());

            // 받은 메시지에 "world"를 붙여서 응답
            String responseMessage = rxValue + " world";
            Serial.printf("[반복:%d | 진행:3/4] 응답할 메시지 : %s\n", messageCounter, responseMessage.c_str());
            pCharacteristic->setValue(responseMessage);
            pCharacteristic->notify();
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(100); // 시리얼 통신 초기화 후 딜레이

    // BLE 초기화
    BLEDevice::init("ESP32-Receiver");
    
    // 서버 생성
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // 서비스 생성
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // 특성 생성
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->addDescriptor(new BLE2902());

    // 서비스 시작
    pService->start();

    // 광고 시작
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE 서버가 시작되었습니다. 트랜스미터의 연결을 기다립니다...");
}

void loop() {
    delay(500); // CPU 부하 감소
}
