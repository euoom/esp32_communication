#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEAddress.h>

// 서비스와 특성 UUID 정의
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_NOTIFY "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// 필수 전역 변수 선언
BLEClient* pClient = nullptr;
BLEAddress* pServerAddress = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristicNotify = nullptr;
bool doConnect = false;
bool connected = false;
uint8_t notificationOn[2] = {0x01, 0x00};

// 메시지 카운터 추가
uint32_t messageCounter = 0;
uint32_t retryCount = 0;
const uint32_t MAX_RETRY_COUNT = 5;
const uint32_t RETRY_DELAY = 2000; // 2초

// 리시버의 MAC 주소
const char* RECEIVER_MAC = "dc:06:75:67:f5:ee";

// 에러 상태 정의
enum ErrorState {
    NO_ERROR = 0,
    SCAN_ERROR = 1,
    CONNECTION_ERROR = 2,
    SERVICE_ERROR = 3,
    CHARACTERISTIC_ERROR = 4,
    NOTIFICATION_ERROR = 5
};

ErrorState currentError = NO_ERROR;

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
            if (!pBLEScan->start(5)) {
                currentError = SCAN_ERROR;
                Serial.println("스캔 실패, 재시도 중...");
                delay(1000);
                return;
            }
        }
        
        if (doConnect) {
            if (retryCount < MAX_RETRY_COUNT) {
                if (connectToServer()) {
                    Serial.println("리시버에 연결되었습니다!");
                    connected = true;
                    retryCount = 0;
                    currentError = NO_ERROR;
                } else {
                    retryCount++;
                    Serial.printf("연결 실패 (시도 %d/%d), 재시도 중...\n", retryCount, MAX_RETRY_COUNT);
                    delay(RETRY_DELAY);
                }
            } else {
                Serial.println("최대 재시도 횟수 초과. 재시작합니다.");
                ESP.restart();
            }
            doConnect = false;
        }
    } else {
        static unsigned long lastSendTime = 0;
        unsigned long currentTime = millis();

        // 5초마다 메시지 전송
        if (currentTime - lastSendTime >= 5000) {
            if (pClient->isConnected()) {
                messageCounter++;
                Serial.printf("[반복:%d | 진행:1/4] 전송할 메시지 : hello\n", messageCounter);
                
                if (!pRemoteCharacteristic->writeValue("hello")) {
                    Serial.println("메시지 전송 실패");
                    connected = false;
                    pClient->disconnect();
                    return;
                }
                
                lastSendTime = currentTime;
            } else {
                Serial.println("연결이 끊어졌습니다. 재연결 시도 중...");
                connected = false;
                pClient->disconnect();
                retryCount = 0;
            }
        }
    }

    delay(500);
}

bool connectToServer() {
    if (!pClient->connect(*pServerAddress)) {
        currentError = CONNECTION_ERROR;
        Serial.println("연결 실패");
        return false;
    }

    // 서비스와 특성 찾기
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        currentError = SERVICE_ERROR;
        Serial.println("서비스를 찾을 수 없습니다");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
        currentError = CHARACTERISTIC_ERROR;
        Serial.println("특성을 찾을 수 없습니다");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristicNotify = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_NOTIFY);
    if (pRemoteCharacteristicNotify == nullptr) {
        currentError = CHARACTERISTIC_ERROR;
        Serial.println("알림 특성을 찾을 수 없습니다");
        pClient->disconnect();
        return false;
    }

    // 알림 활성화
    if (!pRemoteCharacteristicNotify->canNotify()) {
        currentError = NOTIFICATION_ERROR;
        Serial.println("알림을 지원하지 않습니다");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristicNotify->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                               uint8_t* pData, 
                                               size_t length, 
                                               bool isNotify) {
        String receivedMessage = String((char*)pData, length);
        Serial.printf("[반복:%d | 진행:4/4] 수신한 메시지 : %s\n", messageCounter, receivedMessage.c_str());
    });

    if (!pRemoteCharacteristicNotify->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue(notificationOn, 2, true)) {
        currentError = NOTIFICATION_ERROR;
        Serial.println("알림 활성화 실패");
        pClient->disconnect();
        return false;
    }

    return true;
}
