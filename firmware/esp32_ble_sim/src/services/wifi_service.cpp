#include "services/wifi_service.h"
#include "services/config_service.h"
#include "device_state.h"
#include "config.h"
#include <WiFi.h>

WiFiService& WiFiService::getInstance() {
    static WiFiService instance;
    return instance;
}

String WiFiService::getIP() const {
    if (wifiConnected) {
        return WiFi.localIP().toString();
    }
    return "";
}

void WiFiService::setup() {
    // Disable ESP32's automatic WiFi persistence - we manage our own config
    WiFi.persistent(false);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);

    // Force clean state on every boot
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);

    if (configService.isConfigured()) {
        WiFi.mode(WIFI_STA);
        Serial.println("Starting in STA mode (configured)");
    } else {
        startAPMode();
    }
}

void WiFiService::loop() {
    connectToWiFi();
}

void WiFiService::reconnect() {
    wifiRetryCount = 0;
    lastWifiAttempt = 0;
}

void WiFiService::startAP() {
    startAPMode();
}

void WiFiService::stopAP() {
    stopAPMode();
}

void WiFiService::startAPMode() {
    if (apModeActive) return;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    String apName = configService.getAPName();
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

void WiFiService::stopAPMode() {
    if (!apModeActive) return;

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apModeActive = false;

    Serial.println("Access Point stopped (WiFi connected)");
}

void WiFiService::connectToWiFi() {
    if (!configService.isConfigured()) {
        if (!apModeActive) {
            startAPMode();
        }
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            wifiRetryCount = 0;
            Serial.print("WiFi connected! IP: ");
            Serial.println(WiFi.localIP());

            deviceState.setWifiConnected(true, WiFi.localIP().toString());
            stopAPMode();
        }
        return;
    }

    // WiFi not connected
    if (wifiConnected) {
        wifiConnected = false;
        deviceState.setWifiConnected(false);
        wifiRetryCount = 0;
        Serial.println("WiFi disconnected!");
        startAPMode();
    }

    // Don't spam connection attempts
    if (millis() - lastWifiAttempt < WIFI_CONNECT_TIMEOUT) {
        return;
    }
    lastWifiAttempt = millis();

    // Cap retry count
    if (wifiRetryCount < MAX_WIFI_RETRIES) {
        wifiRetryCount++;
    }

    // Start AP mode after too many failed attempts
    if (wifiRetryCount >= MAX_WIFI_RETRIES && !apModeActive) {
        Serial.println("WiFi connection failed after multiple attempts");
        Serial.println("Starting AP mode for reconfiguration...");

        WiFi.mode(WIFI_AP_STA);
        WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
        String apName = configService.getAPName();
        WiFi.softAP(apName.c_str(), AP_PASSWORD);
        apModeActive = true;

        Serial.println("\n========================================");
        Serial.println("Access Point Started");
        Serial.print("  SSID: ");
        Serial.println(apName);
        Serial.print("  Config URL: http://");
        Serial.println(WiFi.softAPIP());
        Serial.println("========================================\n");

        return;
    }

    Serial.print("Connecting to WiFi (attempt ");
    Serial.print(wifiRetryCount);
    Serial.print("/");
    Serial.print(MAX_WIFI_RETRIES);
    Serial.print("): ");
    Serial.println(configService.getWifiSsid());

    if (!apModeActive) {
        WiFi.disconnect(false);
        delay(100);
    }

    WiFi.begin(
        configService.getWifiSsid().c_str(),
        configService.getWifiPassword().c_str()
    );
}
