#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Firmware Version
// ============================================
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// AP Mode Configuration
// ============================================
#define AP_SSID_PREFIX "BLE-Sim-"  // Will append chip ID for uniqueness
#define AP_PASSWORD ""              // Open network (or set a password)
#define AP_IP IPAddress(192, 168, 4, 1)
#define AP_GATEWAY IPAddress(192, 168, 4, 1)
#define AP_SUBNET IPAddress(255, 255, 255, 0)

// ============================================
// Default Values (used until configured)
// ============================================
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_DEVICE_ID_PREFIX "esp32-"

// ============================================
// Timing Configuration
// ============================================
#define BLE_NOTIFY_INTERVAL 1000      // BLE notification interval (ms)
#define MQTT_RECONNECT_INTERVAL 5000  // MQTT reconnect attempt interval (ms)
#define STATUS_REPORT_INTERVAL 10000  // Status report to MQTT (ms)
#define WIFI_CONNECT_TIMEOUT 15000    // WiFi connection timeout (ms)

// ============================================
// NVS Configuration Keys
// ============================================
#define NVS_NAMESPACE "ble-sim"

#endif // CONFIG_H
