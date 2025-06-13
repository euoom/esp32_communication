#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEAddress.h>

// 서비스와 특성 UUID 정의
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// 필수 전역 변수 선언
BLEClient* pClient = nullptr;
BLEAddress* pServerAddress = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool doConnect = false;
bool connected = false;
uint8_t notificationOn[2] = {0x01, 0x00};

// 메시지 카운터 추가
uint32_t messageCounter = 0;

// 리시버의 MAC 주소
const char* RECEIVER_MAC = "dc:06:75:67:f5:ee";

// 콜백 클래스 정의
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.getAddress().toString() == RECEIVER_MAC) {
            advertisedDevice.getScan()->stop();
            pServerAddress = new BLEAddress(advertisedDevice.getAddress());
            doConnect = true;
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(100); // 시리얼 통신 초기화 후 딜레이

    // BLE 초기화
    BLEDevice::init("ESP32-Transmitter");
    pClient = BLEDevice::createClient();
    
    // 스캔 시작
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5);
}

void loop() {
    if (!connected) {
        if (!doConnect) {
            Serial.println("리시버 스캔 중...");
            BLEScan* pBLEScan = BLEDevice::getScan();
            pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
            pBLEScan->setActiveScan(true);
            pBLEScan->start(5);
        }
        
        if (doConnect) {
            if (connectToServer()) {
                Serial.println("리시버에 연결되었습니다!");
                connected = true;
            } else {
                Serial.println("연결 실패, 재시도 중...");
                delay(1000); // 재연결 시도 전 1초 대기
            }
            doConnect = false;
        }
    } else {
        static unsigned long lastSendTime = 0;
        unsigned long currentTime = millis();

        // 5초마다 메시지 전송
        if (currentTime - lastSendTime >= 5000) {
            if (pClient->isConnected()) {
                messageCounter++;  // 카운터 증가
                Serial.printf("[반복:%d | 진행:1/4] 전송할 메시지 : hello\n", messageCounter);
                pRemoteCharacteristic->writeValue("hello");
                lastSendTime = currentTime;
            } else {
                Serial.println("연결이 끊어졌습니다. 재연결 시도 중...");
                connected = false;
                pClient->disconnect();
            }
        }
    }

    delay(500); // CPU 부하 감소
}

bool connectToServer() {
    if (!pClient->connect(*pServerAddress)) {
        Serial.println("연결 실패");
        return false;
    }

    // 서비스와 특성 찾기
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("서비스를 찾을 수 없습니다");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("특성을 찾을 수 없습니다");
        pClient->disconnect();
        return false;
    }

    // 알림 활성화
    pRemoteCharacteristic->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                               uint8_t* pData, 
                                               size_t length, 
                                               bool isNotify) {
        String receivedMessage = String((char*)pData, length);
        Serial.printf("[반복:%d | 진행:4/4] 수신한 메시지 : %s\n", messageCounter, receivedMessage.c_str());
    });

    pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue(notificationOn, 2, true);
    return true;
}
