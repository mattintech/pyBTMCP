#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>

// ============================================
// WiFi Service
// Manages WiFi STA and AP modes
// ============================================
class WiFiService {
public:
    static WiFiService& getInstance();

    // Prevent copying
    WiFiService(const WiFiService&) = delete;
    WiFiService& operator=(const WiFiService&) = delete;

    // Lifecycle
    void setup();
    void loop();

    // State queries
    bool isConnected() const { return wifiConnected; }
    bool isApActive() const { return apModeActive; }
    String getIP() const;

    // Actions
    void startAP();
    void stopAP();
    void reconnect();  // Trigger reconnection attempt

private:
    WiFiService() = default;

    bool wifiConnected = false;
    bool apModeActive = false;
    unsigned long lastWifiAttempt = 0;
    int wifiRetryCount = 0;

    static const int MAX_WIFI_RETRIES = 5;

    void connectToWiFi();
    void startAPMode();
    void stopAPMode();
};

#define wifiService WiFiService::getInstance()

#endif // WIFI_SERVICE_H
