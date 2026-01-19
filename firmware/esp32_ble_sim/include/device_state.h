#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <Arduino.h>
#include <functional>

// ============================================
// Device Types
// ============================================
enum class DeviceType {
    NONE,
    HEART_RATE,
    TREADMILL
};

// ============================================
// Connection State
// ============================================
struct ConnectionState {
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool bleClientConnected = false;
    String ipAddress = "";
};

// ============================================
// Simulated Device Values
// ============================================
struct SimulatedValues {
    // Heart Rate Monitor
    uint8_t heartRate = 70;
    uint8_t batteryLevel = 100;

    // Treadmill
    uint16_t treadmillSpeed = 0;      // 0.01 km/h resolution
    int16_t treadmillIncline = 0;     // 0.1% resolution
    uint32_t treadmillDistance = 0;   // meters
    float distanceAccumulator = 0.0;  // fractional distance
};

// ============================================
// Event Callbacks
// ============================================
using DeviceTypeChangedCallback = std::function<void(DeviceType newType)>;
using ValuesChangedCallback = std::function<void(const SimulatedValues& values)>;
using ConnectionChangedCallback = std::function<void(const ConnectionState& state)>;

// ============================================
// Device State Manager (Singleton)
// ============================================
class DeviceState {
public:
    static DeviceState& getInstance();

    // Prevent copying
    DeviceState(const DeviceState&) = delete;
    DeviceState& operator=(const DeviceState&) = delete;

    // Device type
    DeviceType getDeviceType() const { return deviceType; }
    void setDeviceType(DeviceType type);
    bool isBleStarted() const { return deviceType != DeviceType::NONE; }
    const char* getDeviceTypeString() const;

    // Simulated values
    const SimulatedValues& getValues() const { return values; }
    void setHeartRate(uint8_t bpm);
    void setBatteryLevel(uint8_t level);
    void setTreadmillSpeed(float speedKmh);
    void setTreadmillIncline(float inclinePercent);
    void setTreadmillDistance(uint32_t meters);
    void resetTreadmillDistance();
    void accumulateTreadmillDistance(float deltaSeconds);

    // Connection state
    const ConnectionState& getConnectionState() const { return connectionState; }
    void setWifiConnected(bool connected, const String& ip = "");
    void setMqttConnected(bool connected);
    void setBleClientConnected(bool connected);

    // Event registration
    void onDeviceTypeChanged(DeviceTypeChangedCallback callback);
    void onValuesChanged(ValuesChangedCallback callback);
    void onConnectionChanged(ConnectionChangedCallback callback);

private:
    DeviceState() = default;

    DeviceType deviceType = DeviceType::NONE;
    SimulatedValues values;
    ConnectionState connectionState;

    // Callbacks
    DeviceTypeChangedCallback deviceTypeCallback = nullptr;
    ValuesChangedCallback valuesCallback = nullptr;
    ConnectionChangedCallback connectionCallback = nullptr;

    void notifyValuesChanged();
    void notifyConnectionChanged();
};

// Convenience macro for accessing singleton
#define deviceState DeviceState::getInstance()

#endif // DEVICE_STATE_H
