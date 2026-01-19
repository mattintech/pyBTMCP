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

// Track created services for cleanup
static NimBLEService* pHeartRateService = nullptr;
static NimBLEService* pBatteryService = nullptr;
static NimBLEService* pFitnessMachineService = nullptr;

static bool bleInitialized = false;
static uint16_t currentConnId = 0;              // Track connection ID for disconnect
static bool advertisingPausedFlag = false;      // Shared with callbacks

// Forward declare for callback
static void onBleConnect(NimBLEServer* server);
static void onBleDisconnect();

// ============================================
// Server Callbacks
// ============================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        onBleConnect(pServer);
    }

    void onDisconnect(NimBLEServer* pServer) override {
        onBleDisconnect();
    }
};

static void onBleConnect(NimBLEServer* server) {
    // Track connection ID for forced disconnect
    // getPeerInfo(0) gets the first (most recent) connected peer
    NimBLEConnInfo peerInfo = server->getPeerInfo(0);
    currentConnId = peerInfo.getConnHandle();
    deviceState.setBleClientConnected(true);
    Serial.print("BLE client connected (connId: ");
    Serial.print(currentConnId);
    Serial.println(")");
}

static void onBleDisconnect() {
    currentConnId = 0;
    deviceState.setBleClientConnected(false);
    Serial.println("BLE client disconnected");

    // Only auto-resume advertising if not paused
    if (!advertisingPausedFlag) {
        NimBLEDevice::startAdvertising();
    }
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
    // Check if BLE should be reinitialized after teardown
    if (teardownPending && teardownResumeTime > 0 && millis() >= teardownResumeTime) {
        teardownResumeTime = 0;
        teardownPending = false;
        reinitBLE();
    }

    if (!deviceState.isBleStarted()) return;

    // Check if advertising should be resumed after timed pause
    if (advertisingPaused && advertisingResumeTime > 0 && millis() >= advertisingResumeTime) {
        advertisingResumeTime = 0;
        advertisingPaused = false;
        advertisingPausedFlag = false;
        NimBLEDevice::startAdvertising();
        Serial.println("Advertising resumed after timed pause");
    }

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
        // Clear all service UUIDs from advertising
        pAdvertising->reset();
    }

    // Remove existing services from the server
    if (pServer) {
        if (pHeartRateService) {
            pServer->removeService(pHeartRateService);
            pHeartRateService = nullptr;
        }
        if (pBatteryService) {
            pServer->removeService(pBatteryService);
            pBatteryService = nullptr;
        }
        if (pFitnessMachineService) {
            pServer->removeService(pFitnessMachineService);
            pFitnessMachineService = nullptr;
        }
    }

    // Clear characteristic pointers
    pHeartRateMeasurement = nullptr;
    pBatteryLevel = nullptr;
    pTreadmillData = nullptr;

    Serial.println("BLE stopped and services cleaned up");
}

// ============================================
// Heart Rate Service Setup
// ============================================
void BleService::setupHeartRate() {
    Serial.println("Setting up Heart Rate Service...");

    // Clean up any existing services first
    stop();

    // Create Heart Rate Service
    pHeartRateService = pServer->createService(HEART_RATE_SERVICE_UUID);

    pHeartRateMeasurement = pHeartRateService->createCharacteristic(
        HEART_RATE_MEASUREMENT_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pBodySensorLocation = pHeartRateService->createCharacteristic(
        BODY_SENSOR_LOCATION_UUID,
        NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLocation = 1; // Chest
    pBodySensorLocation->setValue(&sensorLocation, 1);

    pHeartRateService->start();

    // Create Battery Service
    pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);

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

    // Clean up any existing services first
    stop();

    pFitnessMachineService = pServer->createService(FITNESS_MACHINE_SERVICE_UUID);

    NimBLECharacteristic* pFeature = pFitnessMachineService->createCharacteristic(
        FITNESS_MACHINE_FEATURE_UUID,
        NIMBLE_PROPERTY::READ
    );

    uint8_t featureData[8] = {
        0x0B, 0x20, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    pFeature->setValue(featureData, 8);

    pTreadmillData = pFitnessMachineService->createCharacteristic(
        TREADMILL_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    pFitnessMachineService->start();

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

// ============================================
// Disconnect Simulation
// ============================================
void BleService::disconnectClient() {
    if (!pServer || !deviceState.getConnectionState().bleClientConnected) {
        Serial.println("No BLE client connected to disconnect");
        return;
    }

    Serial.println("Forcing BLE client disconnect (immediate re-advertise)");
    pServer->disconnect(currentConnId);
    // onBleDisconnect callback will handle re-advertising
}

void BleService::disconnectClientForDuration(int ms) {
    if (!pServer || !deviceState.getConnectionState().bleClientConnected) {
        Serial.println("No BLE client connected to disconnect");
        return;
    }

    Serial.print("Forcing BLE client disconnect, pausing advertising for ");
    Serial.print(ms);
    Serial.println("ms");

    // Set flags before disconnect so callback knows not to auto-resume
    advertisingPaused = true;
    advertisingPausedFlag = true;
    advertisingResumeTime = millis() + ms;

    pServer->disconnect(currentConnId);
    // onBleDisconnect callback will NOT resume advertising due to flag
}

void BleService::teardownForDuration(int ms) {
    Serial.print("Tearing down BLE stack, will reinit in ");
    Serial.print(ms);
    Serial.println("ms");

    // Clear all service/characteristic pointers
    pHeartRateMeasurement = nullptr;
    pBatteryLevel = nullptr;
    pTreadmillData = nullptr;
    pHeartRateService = nullptr;
    pBatteryService = nullptr;
    pFitnessMachineService = nullptr;
    pServer = nullptr;
    pAdvertising = nullptr;

    // Full BLE deinit
    NimBLEDevice::deinit(true);
    bleInitialized = false;
    currentConnId = 0;
    deviceConnected = false;
    deviceState.setBleClientConnected(false);

    // Schedule reinit
    teardownPending = true;
    teardownResumeTime = millis() + ms;

    Serial.println("BLE stack torn down - device will disappear from scans");
}

void BleService::reinitBLE() {
    Serial.println("Reinitializing BLE stack after teardown...");

    // Reinit the BLE stack
    initBLE();

    // Restore the previous device type configuration
    DeviceType currentType = deviceState.getDeviceType();
    if (currentType == DeviceType::HEART_RATE) {
        setupHeartRate();
        Serial.println("Restored Heart Rate service");
    } else if (currentType == DeviceType::TREADMILL) {
        setupTreadmill();
        Serial.println("Restored Treadmill service");
    }

    Serial.println("BLE stack reinitialized - device visible again");
}
