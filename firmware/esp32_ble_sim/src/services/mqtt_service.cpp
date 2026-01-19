#include "services/mqtt_service.h"
#include "services/config_service.h"
#include "services/wifi_service.h"
#include "services/ble_service.h"
#include "device_state.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT client (needs WiFiClient)
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

MqttService& MqttService::getInstance() {
    static MqttService instance;
    return instance;
}

void MqttService::messageCallback(char* topic, byte* payload, unsigned int length) {
    getInstance().handleMessage(topic, payload, length);
}

void MqttService::setup() {
    mqtt.setCallback(messageCallback);
    mqtt.setBufferSize(512);
}

void MqttService::loop() {
    connectToMQTT();
    mqtt.loop();

    // Periodic status and values report
    if (mqttConnected && millis() - lastStatusReport >= STATUS_REPORT_INTERVAL) {
        lastStatusReport = millis();
        publishStatus();
        publishValues();  // Include current values (e.g., accumulated distance)
    }
}

void MqttService::connectToMQTT() {
    if (!configService.isConfigured() || !wifiService.isConnected()) {
        return;
    }

    if (mqtt.connected()) {
        if (!mqttConnected) {
            mqttConnected = true;
            deviceState.setMqttConnected(true);
        }
        return;
    }

    // Don't spam connection attempts
    if (millis() - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) {
        return;
    }
    lastMqttAttempt = millis();

    mqttConnected = false;
    deviceState.setMqttConnected(false);

    mqtt.setServer(
        configService.getMqttHost().c_str(),
        configService.getMqttPort()
    );

    Serial.print("Connecting to MQTT at ");
    Serial.print(configService.getMqttHost());
    Serial.print(":");
    Serial.println(configService.getMqttPort());

    String clientId = String("esp32-") + String(random(0xffff), HEX);

    // LWT for disconnect detection
    String statusTopic = String("ble-sim/") + configService.getDeviceId() + "/status";
    String willMessage = "{\"online\":false}";

    if (mqtt.connect(clientId.c_str(), statusTopic.c_str(), 1, true, willMessage.c_str())) {
        mqttConnected = true;
        deviceState.setMqttConnected(true);
        Serial.println("MQTT connected with LWT!");

        // Subscribe to control topics
        String configTopic = String("ble-sim/") + configService.getDeviceId() + "/config";
        String setTopic = String("ble-sim/") + configService.getDeviceId() + "/set";
        String disconnectTopic = String("ble-sim/") + configService.getDeviceId() + "/disconnect";

        mqtt.subscribe(configTopic.c_str());
        mqtt.subscribe(setTopic.c_str());
        mqtt.subscribe(disconnectTopic.c_str());

        Serial.print("Subscribed to: ");
        Serial.println(configTopic);

        publishStatus();
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqtt.state());
    }
}

void MqttService::handleMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    String topicStr = String(topic);
    String baseTopic = String("ble-sim/") + configService.getDeviceId();

    // Handle device type configuration
    if (topicStr == baseTopic + "/config") {
        String type = doc["type"] | "";

        Serial.print("Configuring as: ");
        Serial.println(type);

        if (type == "heart_rate") {
            deviceState.setDeviceType(DeviceType::HEART_RATE);
            bleService.setupHeartRate();
        } else if (type == "treadmill") {
            deviceState.setDeviceType(DeviceType::TREADMILL);
            bleService.setupTreadmill();
        } else {
            deviceState.setDeviceType(DeviceType::NONE);
            bleService.stop();
        }

        publishStatus();
    }
    // Handle value updates
    else if (topicStr == baseTopic + "/set") {
        if (doc["heart_rate"].is<int>()) {
            deviceState.setHeartRate(doc["heart_rate"]);
            Serial.print("Heart rate set to: ");
            Serial.println(deviceState.getValues().heartRate);
        }
        if (doc["battery"].is<int>()) {
            deviceState.setBatteryLevel(doc["battery"]);
            bleService.updateBattery(deviceState.getValues().batteryLevel);
            Serial.print("Battery level set to: ");
            Serial.println(deviceState.getValues().batteryLevel);
        }
        if (doc["speed"].is<float>()) {
            deviceState.setTreadmillSpeed(doc["speed"]);
            Serial.print("Speed set to: ");
            Serial.println(doc["speed"].as<float>());
        }
        if (doc["incline"].is<float>()) {
            deviceState.setTreadmillIncline(doc["incline"]);
            Serial.print("Incline set to: ");
            Serial.println(doc["incline"].as<float>());
        }
        if (doc["distance"].is<int>()) {
            deviceState.setTreadmillDistance(doc["distance"]);
            Serial.print("Distance set to: ");
            Serial.println(deviceState.getValues().treadmillDistance);
        }

        publishValues();
    }
    // Handle BLE disconnect command
    else if (topicStr == baseTopic + "/disconnect") {
        int duration = doc["duration_ms"] | 0;  // 0 = immediate resume
        bool teardown = doc["teardown"] | false;  // Full BLE stack teardown

        if (teardown) {
            // Full teardown - device disappears from scans
            int teardownDuration = duration > 0 ? duration : 3000;  // Default 3s for teardown
            bleService.teardownForDuration(teardownDuration);
        } else if (duration > 0) {
            // Just disconnect and pause advertising
            bleService.disconnectClientForDuration(duration);
        } else {
            // Just disconnect, immediate re-advertise
            bleService.disconnectClient();
        }
    }
}

void MqttService::publishStatus() {
    if (!mqtt.connected()) return;

    JsonDocument doc;
    doc["online"] = true;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["type"] = deviceState.getDeviceTypeString();
    doc["ble_started"] = deviceState.isBleStarted();
    doc["ip"] = wifiService.getIP();

    String payload;
    serializeJson(doc, payload);

    String topic = String("ble-sim/") + configService.getDeviceId() + "/status";
    mqtt.publish(topic.c_str(), payload.c_str(), true);
}

void MqttService::publishValues() {
    if (!mqtt.connected()) return;

    JsonDocument doc;
    const auto& values = deviceState.getValues();

    if (deviceState.getDeviceType() == DeviceType::HEART_RATE) {
        doc["heart_rate"] = values.heartRate;
        doc["battery"] = values.batteryLevel;
    } else if (deviceState.getDeviceType() == DeviceType::TREADMILL) {
        doc["speed"] = values.treadmillSpeed / 100.0;
        doc["incline"] = values.treadmillIncline / 10.0;
        doc["distance"] = values.treadmillDistance;
    }

    String payload;
    serializeJson(doc, payload);

    String topic = String("ble-sim/") + configService.getDeviceId() + "/values";
    mqtt.publish(topic.c_str(), payload.c_str());
}
