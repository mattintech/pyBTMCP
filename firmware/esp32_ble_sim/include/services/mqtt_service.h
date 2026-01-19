#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <Arduino.h>

// ============================================
// MQTT Service
// Manages MQTT connection and message routing
// ============================================
class MqttService {
public:
    static MqttService& getInstance();

    // Prevent copying
    MqttService(const MqttService&) = delete;
    MqttService& operator=(const MqttService&) = delete;

    // Lifecycle
    void setup();
    void loop();

    // State
    bool isConnected() const { return mqttConnected; }

    // Publishing
    void publishStatus();
    void publishValues();

private:
    MqttService() = default;

    bool mqttConnected = false;
    unsigned long lastMqttAttempt = 0;
    unsigned long lastStatusReport = 0;

    void connectToMQTT();
    void handleMessage(char* topic, byte* payload, unsigned int length);

    // Static callback wrapper for PubSubClient
    static void messageCallback(char* topic, byte* payload, unsigned int length);
};

#define mqttService MqttService::getInstance()

#endif // MQTT_SERVICE_H
