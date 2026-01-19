#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>

/**
 * Initialize and start the web portal
 * Runs on AP interface at 192.168.4.1
 */
void setupWebPortal();

/**
 * Handle web portal requests (call in loop)
 */
void handleWebPortal();

/**
 * Get status info for web portal display
 */
struct PortalStatus {
    bool wifiConnected;
    bool mqttConnected;
    String ipAddress;
    String deviceType;
    bool bleStarted;
};

void updatePortalStatus(const PortalStatus& status);

#endif // WEB_PORTAL_H
