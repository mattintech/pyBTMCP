#include "services/ble_service.h"
#include "device_state.h"
#include "config.h"
#include <NimBLEDevice.h>

// ============================================
// BLE Server and Characteristics
// ============================================
static NimBLEServer* pServer = nullptr;
static NimBLEAdvertising* pAdvertising = nullptr;
static NimBLECharacteristic* pHeartRateMeasurement = nullptr;
static NimBLECharacteristic* pBatteryLevel = nullptr;
static NimBLECharacteristic* pTreadmillData = nullptr;

static bool bleInitialized = false;

// Forward declare for callback
static void onBleConnect();
static void onBleDisconnect();

// ============================================
// Server Callbacks
// ============================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        onBleConnect();
    }

    void onDisconnect(NimBLEServer* pServer) override {
        onBleDisconnect();
    }
};

static void onBleConnect() {
    deviceState.setBleClientConnected(true);
    Serial.println("BLE client connected");
}

static void onBleDisconnect() {
    deviceState.setBleClientConnected(false);
    Serial.println("BLE client disconnected");
    NimBLEDevice::startAdvertising();
}

// ============================================
// Singleton
// ============================================
BleService& BleService::getInstance() {
    static BleService instance;
    return instance;
}

// ============================================
// Lifecycle
// ============================================
void BleService::setup() {
    initBLE();
}

void BleService::loop() {
    if (!deviceState.isBleStarted()) return;

    // Send notifications at regular intervals
    if (millis() - lastNotify >= BLE_NOTIFY_INTERVAL) {
        lastNotify = millis();

        const auto& values = deviceState.getValues();

        if (deviceState.getDeviceType() == DeviceType::HEART_RATE) {
            notifyHeartRate(values.heartRate);
        } else if (deviceState.getDeviceType() == DeviceType::TREADMILL) {
            // Accumulate distance
            deviceState.accumulateTreadmillDistance(BLE_NOTIFY_INTERVAL / 1000.0);
            const auto& updatedValues = deviceState.getValues();
            notifyTreadmill(updatedValues.treadmillSpeed,
                           updatedValues.treadmillIncline,
                           updatedValues.treadmillDistance);
        }
    }
}

void BleService::initBLE() {
    if (bleInitialized) return;

    NimBLEDevice::init("BLE Simulator");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pAdvertising = NimBLEDevice::getAdvertising();

    bleInitialized = true;
    Serial.println("BLE initialized");
}

void BleService::stop() {
    if (pAdvertising) {
        pAdvertising->stop();
    }

    pHeartRateMeasurement = nullptr;
    pBatteryLevel = nullptr;
    pTreadmillData = nullptr;

    Serial.println("BLE stopped");
}

// ============================================
// Heart Rate Service Setup
// ============================================
void BleService::setupHeartRate() {
    Serial.println("Setting up Heart Rate Service...");

    if (pAdvertising) {
        pAdvertising->stop();
    }

    // Create Heart Rate Service
    NimBLEService* pHRService = pServer->createService(HEART_RATE_SERVICE_UUID);

    pHeartRateMeasurement = pHRService->createCharacteristic(
        HEART_RATE_MEASUREMENT_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pBodySensorLocation = pHRService->createCharacteristic(
        BODY_SENSOR_LOCATION_UUID,
        NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLocation = 1; // Chest
    pBodySensorLocation->setValue(&sensorLocation, 1);

    pHRService->start();

    // Create Battery Service
    NimBLEService* pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);

    pBatteryLevel = pBatteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    uint8_t initialBattery = deviceState.getValues().batteryLevel;
    pBatteryLevel->setValue(&initialBattery, 1);

    pBatteryService->start();

    // Configure advertising
    pAdvertising->addServiceUUID(HEART_RATE_SERVICE_UUID);
    pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    NimBLEDevice::setDeviceName("HR Simulator");
    NimBLEDevice::startAdvertising();

    Serial.println("Heart Rate + Battery Services started, advertising...");
}

// ============================================
// Treadmill Service Setup
// ============================================
void BleService::setupTreadmill() {
    Serial.println("Setting up Fitness Machine Service (Treadmill)...");

    if (pAdvertising) {
        pAdvertising->stop();
    }

    NimBLEService* pService = pServer->createService(FITNESS_MACHINE_SERVICE_UUID);

    NimBLECharacteristic* pFeature = pService->createCharacteristic(
        FITNESS_MACHINE_FEATURE_UUID,
        NIMBLE_PROPERTY::READ
    );

    uint8_t featureData[8] = {
        0x0B, 0x20, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    pFeature->setValue(featureData, 8);

    pTreadmillData = pService->createCharacteristic(
        TREADMILL_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    pAdvertising->addServiceUUID(FITNESS_MACHINE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    NimBLEDevice::setDeviceName("Treadmill Sim");
    NimBLEDevice::startAdvertising();

    Serial.println("Fitness Machine Service (Treadmill) started, advertising...");
}

// ============================================
// Notifications
// ============================================
void BleService::notifyHeartRate(uint8_t bpm) {
    if (!pHeartRateMeasurement || !deviceState.getConnectionState().bleClientConnected) return;

    uint8_t hrData[2];
    hrData[0] = 0x00;  // Flags
    hrData[1] = bpm;

    pHeartRateMeasurement->setValue(hrData, 2);
    pHeartRateMeasurement->notify();
}

void BleService::updateBattery(uint8_t level) {
    if (!pBatteryLevel) return;

    if (level > 100) level = 100;
    pBatteryLevel->setValue(&level, 1);
    pBatteryLevel->notify();
}

void BleService::notifyTreadmill(uint16_t speed, int16_t incline, uint32_t distance) {
    if (!pTreadmillData || !deviceState.getConnectionState().bleClientConnected) return;

    uint8_t data[11];

    uint16_t flags = 0x000C;
    data[0] = flags & 0xFF;
    data[1] = (flags >> 8) & 0xFF;

    data[2] = speed & 0xFF;
    data[3] = (speed >> 8) & 0xFF;

    data[4] = distance & 0xFF;
    data[5] = (distance >> 8) & 0xFF;
    data[6] = (distance >> 16) & 0xFF;

    data[7] = incline & 0xFF;
    data[8] = (incline >> 8) & 0xFF;

    int16_t rampAngle = 0;
    data[9] = rampAngle & 0xFF;
    data[10] = (rampAngle >> 8) & 0xFF;

    pTreadmillData->setValue(data, 11);
    pTreadmillData->notify();
}
