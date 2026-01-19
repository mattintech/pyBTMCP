/**
 * ESP32 BLE Device Simulator
 *
 * Simulates BLE fitness devices (Heart Rate Monitor, Treadmill)
 * Controlled via MQTT from pyBTMCP server
 *
 * Features:
 * - AP mode for configuration when not connected to WiFi (192.168.4.1)
 * - Connects to configured WiFi for MQTT control
 * - Configuration stored in NVS (flash once, configure via web)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "config.h"
#include "config_manager.h"
#include "web_portal.h"
#include "ble_services.h"

// ============================================
// Global State
// ============================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Current device configuration
enum DeviceType {
    DEVICE_NONE,
    DEVICE_HEART_RATE,
    DEVICE_TREADMILL
};

DeviceType currentDeviceType = DEVICE_NONE;
bool bleStarted = false;
bool wifiConnected = false;
bool mqttConnected = false;
bool apModeActive = false;

// Simulated values
uint8_t heartRate = 70;
uint8_t batteryLevel = 100;       // Battery level 0-100%
uint16_t treadmillSpeed = 0;      // 0.01 km/h resolution
int16_t treadmillIncline = 0;     // 0.1% resolution
uint32_t treadmillDistance = 0;   // Total distance in meters
float distanceAccumulator = 0.0;  // Fractional distance accumulator

// Timing
unsigned long lastNotify = 0;
unsigned long lastStatus = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;

// ============================================
// WiFi Functions
// ============================================
void startAPMode() {
    if (apModeActive) return;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    String apName = configManager.getAPName();
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    apModeActive = true;

    Serial.println("\n========================================");
    Serial.println("Access Point Started");
    Serial.print("  SSID: ");
    Serial.println(apName);
    Serial.print("  Config URL: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("========================================\n");
}

void stopAPMode() {
    if (!apModeActive) return;

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apModeActive = false;

    Serial.println("Access Point stopped (WiFi connected)");
}

void setupWiFi() {
    if (configManager.isConfigured()) {
        // Start in STA mode, try to connect
        WiFi.mode(WIFI_STA);
        Serial.println("Starting in STA mode (configured)");
    } else {
        // No config - start AP for setup
        startAPMode();
    }
}

void connectToWiFi() {
    if (!configManager.isConfigured()) {
        // Not configured - ensure AP is running
        if (!apModeActive) {
            startAPMode();
        }
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.print("WiFi connected! IP: ");
            Serial.println(WiFi.localIP());

            // Stop AP mode when connected
            stopAPMode();
        }
        return;
    }

    // WiFi not connected
    if (wifiConnected) {
        // Was connected, now disconnected - start AP
        wifiConnected = false;
        mqttConnected = false;
        Serial.println("WiFi disconnected!");
        startAPMode();
    }

    // Don't spam connection attempts
    if (millis() - lastWifiAttempt < WIFI_CONNECT_TIMEOUT) {
        return;
    }
    lastWifiAttempt = millis();

    // If AP is active, switch to AP+STA to allow connection attempts
    if (apModeActive) {
        WiFi.mode(WIFI_AP_STA);
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(configManager.getWifiSsid());

    WiFi.begin(
        configManager.getWifiSsid().c_str(),
        configManager.getWifiPassword().c_str()
    );
}

// ============================================
// MQTT Functions
// ============================================
void publishStatus() {
    if (!mqtt.connected()) return;

    JsonDocument doc;
    doc["online"] = true;
    doc["firmware_version"] = FIRMWARE_VERSION;

    const char* typeStr = "none";
    if (currentDeviceType == DEVICE_HEART_RATE) typeStr = "heart_rate";
    else if (currentDeviceType == DEVICE_TREADMILL) typeStr = "treadmill";
    doc["type"] = typeStr;

    doc["ble_started"] = bleStarted;
    doc["ip"] = WiFi.localIP().toString();

    String payload;
    serializeJson(doc, payload);

    String topic = String("ble-sim/") + configManager.getDeviceId() + "/status";
    mqtt.publish(topic.c_str(), payload.c_str(), true);  // retained
}

void publishValues() {
    if (!mqtt.connected()) return;

    JsonDocument doc;

    if (currentDeviceType == DEVICE_HEART_RATE) {
        doc["heart_rate"] = heartRate;
        doc["battery"] = batteryLevel;
    } else if (currentDeviceType == DEVICE_TREADMILL) {
        doc["speed"] = treadmillSpeed / 100.0;
        doc["incline"] = treadmillIncline / 10.0;
        doc["distance"] = treadmillDistance;
    }

    String payload;
    serializeJson(doc, payload);

    String topic = String("ble-sim/") + configManager.getDeviceId() + "/values";
    mqtt.publish(topic.c_str(), payload.c_str());
}

void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    String topicStr = String(topic);
    String baseTopic = String("ble-sim/") + configManager.getDeviceId();

    // Handle configuration
    if (topicStr == baseTopic + "/config") {
        String type = doc["type"] | "";

        Serial.print("Configuring as: ");
        Serial.println(type);

        if (type == "heart_rate") {
            currentDeviceType = DEVICE_HEART_RATE;
            setupBLE_HeartRate();
            bleStarted = true;
        } else if (type == "treadmill") {
            currentDeviceType = DEVICE_TREADMILL;
            setupBLE_Treadmill();
            bleStarted = true;
        } else {
            currentDeviceType = DEVICE_NONE;
            stopBLE();
            bleStarted = false;
        }

        publishStatus();
    }
    // Handle value updates
    else if (topicStr == baseTopic + "/set") {
        if (doc["heart_rate"].is<int>()) {
            heartRate = doc["heart_rate"];
            Serial.print("Heart rate set to: ");
            Serial.println(heartRate);
        }
        if (doc["battery"].is<int>()) {
            batteryLevel = doc["battery"];
            if (batteryLevel > 100) batteryLevel = 100;
            updateBatteryLevel(batteryLevel);
            Serial.print("Battery level set to: ");
            Serial.println(batteryLevel);
        }
        if (doc["speed"].is<float>()) {
            float speed = doc["speed"];
            treadmillSpeed = (uint16_t)(speed * 100);
            Serial.print("Speed set to: ");
            Serial.println(speed);
        }
        if (doc["incline"].is<float>()) {
            float incline = doc["incline"];
            treadmillIncline = (int16_t)(incline * 10);
            Serial.print("Incline set to: ");
            Serial.println(incline);
        }
        if (doc["distance"].is<int>()) {
            treadmillDistance = doc["distance"];
            distanceAccumulator = (float)treadmillDistance;  // Sync accumulator
            Serial.print("Distance set to: ");
            Serial.println(treadmillDistance);
        }

        publishValues();
    }
}

void setupMQTT() {
    mqtt.setCallback(handleMqttMessage);
    mqtt.setBufferSize(512);
}

void connectToMQTT() {
    if (!configManager.isConfigured() || !wifiConnected) {
        return;
    }

    if (mqtt.connected()) {
        if (!mqttConnected) {
            mqttConnected = true;
        }
        return;
    }

    // Don't spam connection attempts
    if (millis() - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) {
        return;
    }
    lastMqttAttempt = millis();

    mqttConnected = false;

    // Update MQTT server config
    mqtt.setServer(
        configManager.getMqttHost().c_str(),
        configManager.getMqttPort()
    );

    Serial.print("Connecting to MQTT at ");
    Serial.print(configManager.getMqttHost());
    Serial.print(":");
    Serial.println(configManager.getMqttPort());

    String clientId = String("esp32-") + String(random(0xffff), HEX);

    // Set up Last Will and Testament (LWT) for disconnect detection
    String statusTopic = String("ble-sim/") + configManager.getDeviceId() + "/status";
    String willMessage = "{\"online\":false}";

    // Connect with LWT: topic, QoS 1, retain true, message
    if (mqtt.connect(clientId.c_str(), statusTopic.c_str(), 1, true, willMessage.c_str())) {
        mqttConnected = true;
        Serial.println("MQTT connected with LWT!");

        // Subscribe to control topics
        String configTopic = String("ble-sim/") + configManager.getDeviceId() + "/config";
        String setTopic = String("ble-sim/") + configManager.getDeviceId() + "/set";

        mqtt.subscribe(configTopic.c_str());
        mqtt.subscribe(setTopic.c_str());

        Serial.print("Subscribed to: ");
        Serial.println(configTopic);

        // Publish initial status (with retain so new subscribers see current state)
        publishStatus();
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqtt.state());
    }
}

// ============================================
// Reset Treadmill Distance
// ============================================
void resetTreadmillDistance() {
    treadmillDistance = 0;
    distanceAccumulator = 0.0;
    Serial.println("Treadmill distance reset to 0");
}

// ============================================
// Set Battery Level (from web UI)
// ============================================
void setBatteryLevelCallback(uint8_t level) {
    batteryLevel = level;
    updateBatteryLevel(batteryLevel);
    Serial.print("Battery level set via web UI: ");
    Serial.println(batteryLevel);
}

// ============================================
// Update Portal Status
// ============================================
void updateStatus() {
    PortalStatus status;
    status.wifiConnected = wifiConnected;
    status.mqttConnected = mqttConnected;
    status.bleStarted = bleStarted;
    status.ipAddress = wifiConnected ? WiFi.localIP().toString() : "";
    status.treadmillDistance = treadmillDistance;
    status.batteryLevel = batteryLevel;

    if (currentDeviceType == DEVICE_HEART_RATE) {
        status.deviceType = "Heart Rate";
    } else if (currentDeviceType == DEVICE_TREADMILL) {
        status.deviceType = "Treadmill";
    } else {
        status.deviceType = "Not configured";
    }

    updatePortalStatus(status);
}

// ============================================
// Main Setup & Loop
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   ESP32 BLE Device Simulator");
    Serial.print("   Firmware: v");
    Serial.println(FIRMWARE_VERSION);
    Serial.println("========================================\n");

    // Load configuration from NVS
    bool hasConfig = configManager.load();
    Serial.print("Device ID: ");
    Serial.println(configManager.getDeviceId());
    Serial.print("Configured: ");
    Serial.println(hasConfig ? "Yes" : "No");

    // Start WiFi (AP always on, STA if configured)
    setupWiFi();

    // Start web configuration portal
    setupWebPortal();
    setResetDistanceCallback(resetTreadmillDistance);
    setSetBatteryCallback(setBatteryLevelCallback);

    // Initialize MQTT
    setupMQTT();

    // Initialize BLE (don't start advertising yet)
    initBLE();

    Serial.println("\nReady!");
    if (!configManager.isConfigured()) {
        Serial.println("Configure at: http://192.168.4.1");
    } else {
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Waiting for MQTT commands...\n");
}

void loop() {
    // Handle web portal requests
    handleWebPortal();

    // Maintain WiFi STA connection (if configured)
    connectToWiFi();

    // Maintain MQTT connection
    connectToMQTT();
    mqtt.loop();

    // Update portal status display
    updateStatus();

    // Send BLE notifications
    if (bleStarted && millis() - lastNotify >= BLE_NOTIFY_INTERVAL) {
        lastNotify = millis();

        if (currentDeviceType == DEVICE_HEART_RATE) {
            notifyHeartRate(heartRate);
        } else if (currentDeviceType == DEVICE_TREADMILL) {
            // Accumulate distance based on speed
            // Speed is in 0.01 km/h units, interval is 1 second
            // meters per second = (speed/100) * 1000 / 3600 = speed / 360
            distanceAccumulator += treadmillSpeed / 360.0;
            treadmillDistance = (uint32_t)distanceAccumulator;

            notifyTreadmill(treadmillSpeed, treadmillIncline, treadmillDistance);
        }
    }

    // Periodic status report to MQTT
    if (mqttConnected && millis() - lastStatus >= STATUS_REPORT_INTERVAL) {
        lastStatus = millis();
        publishStatus();
    }
}
