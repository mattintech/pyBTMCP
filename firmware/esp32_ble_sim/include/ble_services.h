#ifndef BLE_SERVICES_H
#define BLE_SERVICES_H

#include <Arduino.h>

// ============================================
// BLE Service UUIDs (Bluetooth SIG standard)
// ============================================

// Heart Rate Service
#define HEART_RATE_SERVICE_UUID        "180D"
#define HEART_RATE_MEASUREMENT_UUID    "2A37"
#define BODY_SENSOR_LOCATION_UUID      "2A38"

// Battery Service
#define BATTERY_SERVICE_UUID           "180F"
#define BATTERY_LEVEL_UUID             "2A19"

// Fitness Machine Service
#define FITNESS_MACHINE_SERVICE_UUID   "1826"
#define TREADMILL_DATA_UUID            "2ACD"
#define FITNESS_MACHINE_FEATURE_UUID   "2ACC"

// ============================================
// Function Prototypes
// ============================================

/**
 * Initialize NimBLE stack (call once at startup)
 */
void initBLE();

/**
 * Stop BLE advertising and deinit
 */
void stopBLE();

/**
 * Setup BLE as Heart Rate Monitor
 * Creates Heart Rate Service with measurement characteristic
 */
void setupBLE_HeartRate();

/**
 * Setup BLE as Treadmill (Fitness Machine)
 * Creates Fitness Machine Service with treadmill data
 */
void setupBLE_Treadmill();

/**
 * Send heart rate notification
 * @param bpm Heart rate in beats per minute
 */
void notifyHeartRate(uint8_t bpm);

/**
 * Update battery level
 * @param level Battery level 0-100%
 */
void updateBatteryLevel(uint8_t level);

/**
 * Send treadmill data notification
 * @param speed Speed in 0.01 km/h units
 * @param incline Incline in 0.1% units
 * @param distance Total distance in meters
 */
void notifyTreadmill(uint16_t speed, int16_t incline, uint32_t distance);

#endif // BLE_SERVICES_H
