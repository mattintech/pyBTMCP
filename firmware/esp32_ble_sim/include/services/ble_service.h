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

    // Disconnect simulation
    void disconnectClient();                    // Force disconnect + immediate re-advertise
    void disconnectClientForDuration(int ms);   // Disconnect + pause advertising for duration
    void teardownForDuration(int ms);           // Full BLE deinit + reinit after duration

private:
    BleService() = default;

    bool deviceConnected = false;
    unsigned long lastNotify = 0;
    unsigned long advertisingResumeTime = 0;    // When to resume advertising (0 = not paused)
    bool advertisingPaused = false;
    unsigned long teardownResumeTime = 0;       // When to reinit BLE after teardown
    bool teardownPending = false;

    void initBLE();
    void reinitBLE();                           // Reinit BLE and restore services
};

#define bleService BleService::getInstance()

#endif // BLE_SERVICE_H
