/**
 * BLE Services Implementation
 *
 * Implements standard Bluetooth SIG GATT services:
 * - Heart Rate Service (0x180D)
 * - Fitness Machine Service (0x1826) - Treadmill
 */

#include "ble_services.h"
#include "config.h"
#include "config_manager.h"
#include <NimBLEDevice.h>

// ============================================
// BLE Server and Characteristics
// ============================================
static NimBLEServer* pServer = nullptr;
static NimBLEAdvertising* pAdvertising = nullptr;

// Heart Rate
static NimBLECharacteristic* pHeartRateMeasurement = nullptr;

// Treadmill
static NimBLECharacteristic* pTreadmillData = nullptr;

static bool deviceConnected = false;
static bool oldDeviceConnected = false;

// ============================================
// Server Callbacks
// ============================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        deviceConnected = true;
        Serial.println("BLE client connected");
    }

    void onDisconnect(NimBLEServer* pServer) override {
        deviceConnected = false;
        Serial.println("BLE client disconnected");

        // Restart advertising
        NimBLEDevice::startAdvertising();
    }
};

// ============================================
// BLE Initialization
// ============================================
void initBLE() {
    NimBLEDevice::init("BLE Simulator");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pAdvertising = NimBLEDevice::getAdvertising();

    Serial.println("BLE initialized");
}

void stopBLE() {
    if (pAdvertising) {
        pAdvertising->stop();
    }

    // Clear services (will be recreated on next setup)
    pHeartRateMeasurement = nullptr;
    pTreadmillData = nullptr;

    Serial.println("BLE stopped");
}

// ============================================
// Heart Rate Service Setup
// ============================================
void setupBLE_HeartRate() {
    Serial.println("Setting up Heart Rate Service...");

    // Stop any current advertising
    if (pAdvertising) {
        pAdvertising->stop();
    }

    // Create Heart Rate Service
    NimBLEService* pService = pServer->createService(HEART_RATE_SERVICE_UUID);

    // Heart Rate Measurement Characteristic
    // Flags: Notify
    pHeartRateMeasurement = pService->createCharacteristic(
        HEART_RATE_MEASUREMENT_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Body Sensor Location Characteristic
    // Flags: Read
    // Value: 1 = Chest
    NimBLECharacteristic* pBodySensorLocation = pService->createCharacteristic(
        BODY_SENSOR_LOCATION_UUID,
        NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLocation = 1; // Chest
    pBodySensorLocation->setValue(&sensorLocation, 1);

    // Start the service
    pService->start();

    // Configure advertising
    pAdvertising->addServiceUUID(HEART_RATE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    // Update device name
    NimBLEDevice::setDeviceName("HR Simulator");

    // Start advertising
    NimBLEDevice::startAdvertising();

    Serial.println("Heart Rate Service started, advertising...");
}

// ============================================
// Treadmill (Fitness Machine) Service Setup
// ============================================
void setupBLE_Treadmill() {
    Serial.println("Setting up Fitness Machine Service (Treadmill)...");

    // Stop any current advertising
    if (pAdvertising) {
        pAdvertising->stop();
    }

    // Create Fitness Machine Service
    NimBLEService* pService = pServer->createService(FITNESS_MACHINE_SERVICE_UUID);

    // Fitness Machine Feature Characteristic
    // Flags: Read
    NimBLECharacteristic* pFeature = pService->createCharacteristic(
        FITNESS_MACHINE_FEATURE_UUID,
        NIMBLE_PROPERTY::READ
    );

    // Feature flags for treadmill:
    // Byte 0-3: Fitness Machine Features
    //   Bit 0: Average Speed Supported
    //   Bit 1: Cadence Supported
    //   Bit 2: Total Distance Supported
    //   Bit 3: Inclination Supported
    //   Bit 13: Elapsed Time Supported
    // Byte 4-7: Target Setting Features
    uint8_t featureData[8] = {
        0x0B, 0x20, 0x00, 0x00,  // Features: Speed, Cadence, Distance, Inclination, Elapsed Time
        0x00, 0x00, 0x00, 0x00   // Target settings (none)
    };
    pFeature->setValue(featureData, 8);

    // Treadmill Data Characteristic
    // Flags: Notify
    pTreadmillData = pService->createCharacteristic(
        TREADMILL_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Start the service
    pService->start();

    // Configure advertising
    pAdvertising->addServiceUUID(FITNESS_MACHINE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    // Update device name
    NimBLEDevice::setDeviceName("Treadmill Sim");

    // Start advertising
    NimBLEDevice::startAdvertising();

    Serial.println("Fitness Machine Service (Treadmill) started, advertising...");
}

// ============================================
// Heart Rate Notification
// ============================================
void notifyHeartRate(uint8_t bpm) {
    if (!pHeartRateMeasurement || !deviceConnected) return;

    // Heart Rate Measurement format:
    // Byte 0: Flags
    //   Bit 0: Heart Rate Value Format (0 = UINT8, 1 = UINT16)
    //   Bit 1-2: Sensor Contact Status
    //   Bit 3: Energy Expended Status
    //   Bit 4: RR-Interval
    // Byte 1: Heart Rate Value (UINT8)
    uint8_t hrData[2];
    hrData[0] = 0x00;  // Flags: UINT8 format, no contact detection
    hrData[1] = bpm;

    pHeartRateMeasurement->setValue(hrData, 2);
    pHeartRateMeasurement->notify();
}

// ============================================
// Treadmill Data Notification
// ============================================
void notifyTreadmill(uint16_t speed, int16_t incline) {
    if (!pTreadmillData || !deviceConnected) return;

    // Treadmill Data format (per Bluetooth FTMS spec):
    // Byte 0-1: Flags
    //   Bit 0: More Data (0 = Instantaneous Speed present)
    //   Bit 1: Average Speed present
    //   Bit 2: Total Distance present
    //   Bit 3: Inclination and Ramp Angle present
    // Following bytes: Data fields based on flags

    // We'll include: Instantaneous Speed + Inclination + Ramp Angle
    uint8_t data[8];

    // Flags: Inclination and Ramp Angle present (bit 3)
    uint16_t flags = 0x0008;
    data[0] = flags & 0xFF;
    data[1] = (flags >> 8) & 0xFF;

    // Instantaneous Speed (always present when More Data=0, uint16, 0.01 km/h resolution)
    data[2] = speed & 0xFF;
    data[3] = (speed >> 8) & 0xFF;

    // Inclination (sint16, 0.1% resolution)
    data[4] = incline & 0xFF;
    data[5] = (incline >> 8) & 0xFF;

    // Ramp Angle Setting (sint16, 0.1 degree resolution) - set to 0
    int16_t rampAngle = 0;
    data[6] = rampAngle & 0xFF;
    data[7] = (rampAngle >> 8) & 0xFF;

    pTreadmillData->setValue(data, 8);
    pTreadmillData->notify();
}
