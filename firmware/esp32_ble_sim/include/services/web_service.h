#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include <Arduino.h>

// ============================================
// Web Service
// Manages HTTP server for configuration portal
// ============================================
class WebService {
public:
    static WebService& getInstance();

    // Prevent copying
    WebService(const WebService&) = delete;
    WebService& operator=(const WebService&) = delete;

    // Lifecycle
    void setup();
    void loop();

private:
    WebService() = default;

    void handleRoot();
    void handleGetStatus();
    void handlePostConfig();
    void handleReset();
    void handleResetDistance();
    void handleSetBattery();
};

#define webService WebService::getInstance()

#endif // WEB_SERVICE_H
