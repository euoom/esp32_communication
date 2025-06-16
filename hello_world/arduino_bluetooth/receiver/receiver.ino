#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 서비스와 특성 UUID 정의 (트랜스미터와 동일)
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_NOTIFY "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// BLE 서버 객체
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLECharacteristic* pCharacteristicNotify = nullptr;
bool deviceConnected = false;

// 메시지 카운터 추가
uint32_t messageCounter = 0;
uint32_t errorCount = 0;
const uint32_t MAX_ERROR_COUNT = 5;

// 에러 상태 정의
enum ErrorState {
    NO_ERROR = 0,
    SERVICE_CREATE_ERROR = 1,
    CHARACTERISTIC_CREATE_ERROR = 2,
    NOTIFICATION_SETUP_ERROR = 3,
    ADVERTISING_ERROR = 4
};

ErrorState currentError = NO_ERROR;

// 연결 상태 콜백 클래스
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        errorCount = 0;
        currentError = NO_ERROR;
        Serial.println("트랜스미터가 연결되었습니다!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("트랜스미터 연결이 끊어졌습니다.");
        
        // 재연결을 위해 광고 재시작
        if (!pServer->getAdvertising()->start()) {
            currentError = ADVERTISING_ERROR;
            errorCount++;
            Serial.println("광고 재시작 실패");
            
            if (errorCount >= MAX_ERROR_COUNT) {
                Serial.println("최대 에러 횟수 초과. 재시작합니다.");
                ESP.restart();
            }
        }
    }
};

// 특성 콜백 클래스
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            messageCounter++;
            Serial.printf("[반복:%d | 진행:2/4] 수신한 메시지 : %s\n", messageCounter, rxValue.c_str());

            // 받은 메시지에 "world"를 붙여서 응답
            String responseMessage = rxValue + " world";
            Serial.printf("[반복:%d | 진행:3/4] 응답할 메시지 : %s\n", messageCounter, responseMessage.c_str());
            
            if (!pCharacteristicNotify->setValue(responseMessage)) {
                Serial.println("응답 메시지 설정 실패");
                return;
            }
            
            if (!pCharacteristicNotify->notify()) {
                Serial.println("알림 전송 실패");
                return;
            }
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
    if (pServer == nullptr) {
        Serial.println("서버 생성 실패");
        ESP.restart();
    }
    pServer->setCallbacks(new MyServerCallbacks());

    // 서비스 생성
    BLEService *pService = pServer->createService(SERVICE_UUID);
    if (pService == nullptr) {
        currentError = SERVICE_CREATE_ERROR;
        Serial.println("서비스 생성 실패");
        ESP.restart();
    }

    // 특성 생성 (쓰기용)
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    if (pCharacteristic == nullptr) {
        currentError = CHARACTERISTIC_CREATE_ERROR;
        Serial.println("특성 생성 실패");
        ESP.restart();
    }

    // 특성 생성 (알림용)
    pCharacteristicNotify = pService->createCharacteristic(
        CHARACTERISTIC_UUID_NOTIFY,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    if (pCharacteristicNotify == nullptr) {
        currentError = CHARACTERISTIC_CREATE_ERROR;
        Serial.println("알림 특성 생성 실패");
        ESP.restart();
    }

    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristicNotify->addDescriptor(new BLE2902());

    // 서비스 시작
    if (!pService->start()) {
        currentError = SERVICE_CREATE_ERROR;
        Serial.println("서비스 시작 실패");
        ESP.restart();
    }

    // 광고 시작
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising == nullptr) {
        currentError = ADVERTISING_ERROR;
        Serial.println("광고 객체 생성 실패");
        ESP.restart();
    }

    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    if (!BLEDevice::startAdvertising()) {
        currentError = ADVERTISING_ERROR;
        Serial.println("광고 시작 실패");
        ESP.restart();
    }
    
    Serial.println("BLE 서버가 시작되었습니다. 트랜스미터의 연결을 기다립니다...");
}

void loop() {
    if (currentError != NO_ERROR) {
        errorCount++;
        if (errorCount >= MAX_ERROR_COUNT) {
            Serial.println("최대 에러 횟수 초과. 재시작합니다.");
            ESP.restart();
        }
    }
    delay(500);
}
