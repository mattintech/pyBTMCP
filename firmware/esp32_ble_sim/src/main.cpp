/**
 * ESP32 BLE Device Simulator
 *
 * Simulates BLE fitness devices (Heart Rate Monitor, Treadmill)
 * Controlled via MQTT from pyBTMCP server
 *
 * Architecture:
 * - device_state: Central state management with event callbacks
 * - config_service: NVS persistence for configuration
 * - wifi_service: WiFi STA/AP management
 * - mqtt_service: MQTT client and message routing
 * - ble_service: BLE GATT services and notifications
 * - web_service: HTTP configuration portal
 */

#include <Arduino.h>
#include "config.h"
#include "device_state.h"
#include "services/config_service.h"
#include "services/wifi_service.h"
#include "services/mqtt_service.h"
#include "services/ble_service.h"
#include "services/web_service.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   ESP32 BLE Device Simulator");
    Serial.print("   Firmware: v");
    Serial.println(FIRMWARE_VERSION);
    Serial.println("========================================\n");

    // Load configuration from NVS
    bool hasConfig = configService.load();
    Serial.print("Device ID: ");
    Serial.println(configService.getDeviceId());
    Serial.print("Configured: ");
    Serial.println(hasConfig ? "Yes" : "No");

    // Initialize all services
    wifiService.setup();
    webService.setup();
    mqttService.setup();
    bleService.setup();

    Serial.println("\nReady!");
    if (!configService.isConfigured()) {
        Serial.println("Configure at: http://192.168.4.1");
    } else {
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Waiting for MQTT commands...\n");
}

void loop() {
    // Run all service loops
    wifiService.loop();
    webService.loop();
    mqttService.loop();
    bleService.loop();
}
