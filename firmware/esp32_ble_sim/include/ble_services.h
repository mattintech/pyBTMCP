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
 * Send treadmill data notification
 * @param speed Speed in 0.01 km/h units
 * @param incline Incline in 0.1% units
 */
void notifyTreadmill(uint16_t speed, int16_t incline);

#endif // BLE_SERVICES_H
