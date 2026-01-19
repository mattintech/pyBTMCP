#include "device_state.h"

DeviceState& DeviceState::getInstance() {
    static DeviceState instance;
    return instance;
}

const char* DeviceState::getDeviceTypeString() const {
    switch (deviceType) {
        case DeviceType::HEART_RATE: return "heart_rate";
        case DeviceType::TREADMILL: return "treadmill";
        default: return "";
    }
}

void DeviceState::setDeviceType(DeviceType type) {
    if (deviceType != type) {
        deviceType = type;
        if (deviceTypeCallback) {
            deviceTypeCallback(type);
        }
    }
}

void DeviceState::setHeartRate(uint8_t bpm) {
    values.heartRate = bpm;
    notifyValuesChanged();
}

void DeviceState::setBatteryLevel(uint8_t level) {
    if (level > 100) level = 100;
    values.batteryLevel = level;
    notifyValuesChanged();
}

void DeviceState::setTreadmillSpeed(float speedKmh) {
    values.treadmillSpeed = (uint16_t)(speedKmh * 100);
    notifyValuesChanged();
}

void DeviceState::setTreadmillIncline(float inclinePercent) {
    values.treadmillIncline = (int16_t)(inclinePercent * 10);
    notifyValuesChanged();
}

void DeviceState::setTreadmillDistance(uint32_t meters) {
    values.treadmillDistance = meters;
    values.distanceAccumulator = (float)meters;
    notifyValuesChanged();
}

void DeviceState::resetTreadmillDistance() {
    values.treadmillDistance = 0;
    values.distanceAccumulator = 0.0;
    Serial.println("Treadmill distance reset to 0");
    notifyValuesChanged();
}

void DeviceState::accumulateTreadmillDistance(float deltaSeconds) {
    // Speed is in 0.01 km/h units
    // meters per second = (speed/100) * 1000 / 3600 = speed / 360
    values.distanceAccumulator += (values.treadmillSpeed / 360.0) * deltaSeconds;
    values.treadmillDistance = (uint32_t)values.distanceAccumulator;
}

void DeviceState::setWifiConnected(bool connected, const String& ip) {
    connectionState.wifiConnected = connected;
    connectionState.ipAddress = connected ? ip : "";
    notifyConnectionChanged();
}

void DeviceState::setMqttConnected(bool connected) {
    connectionState.mqttConnected = connected;
    notifyConnectionChanged();
}

void DeviceState::setBleClientConnected(bool connected) {
    connectionState.bleClientConnected = connected;
    notifyConnectionChanged();
}

void DeviceState::onDeviceTypeChanged(DeviceTypeChangedCallback callback) {
    deviceTypeCallback = callback;
}

void DeviceState::onValuesChanged(ValuesChangedCallback callback) {
    valuesCallback = callback;
}

void DeviceState::onConnectionChanged(ConnectionChangedCallback callback) {
    connectionCallback = callback;
}

void DeviceState::notifyValuesChanged() {
    if (valuesCallback) {
        valuesCallback(values);
    }
}

void DeviceState::notifyConnectionChanged() {
    if (connectionCallback) {
        connectionCallback(connectionState);
    }
}
