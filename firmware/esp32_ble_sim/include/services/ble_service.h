#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>

// ============================================
// BLE Service UUIDs (Bluetooth SIG standard)
// ============================================
#define HEART_RATE_SERVICE_UUID        "180D"
#define HEART_RATE_MEASUREMENT_UUID    "2A37"
#define BODY_SENSOR_LOCATION_UUID      "2A38"
#define BATTERY_SERVICE_UUID           "180F"
#define BATTERY_LEVEL_UUID             "2A19"
#define FITNESS_MACHINE_SERVICE_UUID   "1826"
#define TREADMILL_DATA_UUID            "2ACD"
#define FITNESS_MACHINE_FEATURE_UUID   "2ACC"

// ============================================
// BLE Service
// Manages BLE advertising and GATT services
// ============================================
class BleService {
public:
    static BleService& getInstance();

    // Prevent copying
    BleService(const BleService&) = delete;
    BleService& operator=(const BleService&) = delete;

    // Lifecycle
    void setup();
    void loop();

    // Configuration
    void setupHeartRate();
    void setupTreadmill();
    void stop();

    // Notifications
    void notifyHeartRate(uint8_t bpm);
    void notifyTreadmill(uint16_t speed, int16_t incline, uint32_t distance);
    void updateBattery(uint8_t level);

    // State
    bool isClientConnected() const { return deviceConnected; }

private:
    BleService() = default;

    bool deviceConnected = false;
    unsigned long lastNotify = 0;

    void initBLE();
};

#define bleService BleService::getInstance()

#endif // BLE_SERVICE_H
