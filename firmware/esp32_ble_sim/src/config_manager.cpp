#include "config_manager.h"
#include "config.h"

// Global instance
ConfigManager configManager;

ConfigManager::ConfigManager() {}

bool ConfigManager::load() {
    preferences.begin(NVS_NAMESPACE, true);  // Read-only

    config.configured = preferences.getBool("configured", false);
    config.wifiSsid = preferences.getString("wifi_ssid", "");
    config.wifiPassword = preferences.getString("wifi_pass", "");
    config.mqttHost = preferences.getString("mqtt_host", "");
    config.mqttPort = preferences.getUShort("mqtt_port", DEFAULT_MQTT_PORT);
    config.deviceId = preferences.getString("device_id", getDefaultDeviceId());

    preferences.end();

    return config.configured;
}

void ConfigManager::save() {
    preferences.begin(NVS_NAMESPACE, false);  // Read-write

    preferences.putBool("configured", config.configured);
    preferences.putString("wifi_ssid", config.wifiSsid);
    preferences.putString("wifi_pass", config.wifiPassword);
    preferences.putString("mqtt_host", config.mqttHost);
    preferences.putUShort("mqtt_port", config.mqttPort);
    preferences.putString("device_id", config.deviceId);

    preferences.end();

    Serial.println("Configuration saved to NVS");
}

void ConfigManager::clear() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();

    config = DeviceConfig();
    Serial.println("Configuration cleared");
}

void ConfigManager::setWifiCredentials(const String& ssid, const String& password) {
    config.wifiSsid = ssid;
    config.wifiPassword = password;
    if (ssid.length() > 0) {
        config.configured = true;
    }
}

void ConfigManager::setMqttConfig(const String& host, uint16_t port) {
    config.mqttHost = host;
    config.mqttPort = port;
}

void ConfigManager::setDeviceId(const String& id) {
    config.deviceId = id.length() > 0 ? id : getDefaultDeviceId();
}

String ConfigManager::getAPName() const {
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return String(AP_SSID_PREFIX) + String(chipId, HEX);
}

String ConfigManager::getDefaultDeviceId() const {
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return String(DEFAULT_DEVICE_ID_PREFIX) + String(chipId, HEX);
}
