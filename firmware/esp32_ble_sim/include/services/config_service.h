#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <Arduino.h>
#include <Preferences.h>

// ============================================
// Config Service
// Manages persistent configuration in NVS
// ============================================
class ConfigService {
public:
    static ConfigService& getInstance();

    // Prevent copying
    ConfigService(const ConfigService&) = delete;
    ConfigService& operator=(const ConfigService&) = delete;

    // Load config from NVS
    bool load();

    // Save config to NVS
    void save();

    // Clear all config
    void clear();

    // Check if configured
    bool isConfigured() const { return configured; }

    // Getters
    const String& getWifiSsid() const { return wifiSsid; }
    const String& getWifiPassword() const { return wifiPassword; }
    const String& getMqttHost() const { return mqttHost; }
    uint16_t getMqttPort() const { return mqttPort; }
    const String& getDeviceId() const { return deviceId; }

    // Setters
    void setWifiCredentials(const String& ssid, const String& password);
    void setMqttConfig(const String& host, uint16_t port);
    void setDeviceId(const String& id);

    // Get unique AP name based on chip ID
    String getAPName() const;

    // Get unique default device ID based on chip ID
    String getDefaultDeviceId() const;

private:
    ConfigService() = default;

    bool configured = false;
    String wifiSsid = "";
    String wifiPassword = "";
    String mqttHost = "";
    uint16_t mqttPort = 1883;
    String deviceId = "";

    Preferences preferences;
};

#define configService ConfigService::getInstance()

#endif // CONFIG_SERVICE_H
