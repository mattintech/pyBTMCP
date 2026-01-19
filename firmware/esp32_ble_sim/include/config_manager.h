#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// ============================================
// Configuration Structure
// ============================================
struct DeviceConfig {
    bool configured = false;
    String wifiSsid = "";
    String wifiPassword = "";
    String mqttHost = "";
    uint16_t mqttPort = 1883;
    String deviceId = "";
};

// ============================================
// Configuration Manager Class
// ============================================
class ConfigManager {
public:
    ConfigManager();

    // Load config from NVS
    bool load();

    // Save config to NVS
    void save();

    // Clear all config
    void clear();

    // Check if configured
    bool isConfigured() const { return config.configured; }

    // Getters
    const String& getWifiSsid() const { return config.wifiSsid; }
    const String& getWifiPassword() const { return config.wifiPassword; }
    const String& getMqttHost() const { return config.mqttHost; }
    uint16_t getMqttPort() const { return config.mqttPort; }
    const String& getDeviceId() const { return config.deviceId; }

    // Setters
    void setWifiCredentials(const String& ssid, const String& password);
    void setMqttConfig(const String& host, uint16_t port);
    void setDeviceId(const String& id);

    // Get unique AP name based on chip ID
    String getAPName() const;

    // Get unique default device ID based on chip ID
    String getDefaultDeviceId() const;

private:
    DeviceConfig config;
    Preferences preferences;
};

// Global instance
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
