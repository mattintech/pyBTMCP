#include "services/config_service.h"
#include "config.h"

ConfigService& ConfigService::getInstance() {
    static ConfigService instance;
    return instance;
}

bool ConfigService::load() {
    preferences.begin(NVS_NAMESPACE, true);  // Read-only

    configured = preferences.getBool("configured", false);
    wifiSsid = preferences.getString("wifi_ssid", "");
    wifiPassword = preferences.getString("wifi_pass", "");
    mqttHost = preferences.getString("mqtt_host", "");
    mqttPort = preferences.getUShort("mqtt_port", DEFAULT_MQTT_PORT);
    deviceId = preferences.getString("device_id", getDefaultDeviceId());

    preferences.end();

    return configured;
}

void ConfigService::save() {
    preferences.begin(NVS_NAMESPACE, false);  // Read-write

    preferences.putBool("configured", configured);
    preferences.putString("wifi_ssid", wifiSsid);
    preferences.putString("wifi_pass", wifiPassword);
    preferences.putString("mqtt_host", mqttHost);
    preferences.putUShort("mqtt_port", mqttPort);
    preferences.putString("device_id", deviceId);

    preferences.end();

    Serial.println("Configuration saved to NVS");
}

void ConfigService::clear() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();

    configured = false;
    wifiSsid = "";
    wifiPassword = "";
    mqttHost = "";
    mqttPort = DEFAULT_MQTT_PORT;
    deviceId = "";

    Serial.println("Configuration cleared");
}

void ConfigService::setWifiCredentials(const String& ssid, const String& password) {
    wifiSsid = ssid;
    wifiPassword = password;
    if (ssid.length() > 0) {
        configured = true;
    }
}

void ConfigService::setMqttConfig(const String& host, uint16_t port) {
    mqttHost = host;
    mqttPort = port;
}

void ConfigService::setDeviceId(const String& id) {
    deviceId = id.length() > 0 ? id : getDefaultDeviceId();
}

String ConfigService::getAPName() const {
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return String(AP_SSID_PREFIX) + String(chipId, HEX);
}

String ConfigService::getDefaultDeviceId() const {
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return String(DEFAULT_DEVICE_ID_PREFIX) + String(chipId, HEX);
}
