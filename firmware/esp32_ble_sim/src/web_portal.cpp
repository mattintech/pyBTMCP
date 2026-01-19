#include "web_portal.h"
#include "config_manager.h"
#include <WebServer.h>
#include <ArduinoJson.h>

WebServer server(80);
PortalStatus currentStatus = {};

// HTML template with embedded CSS and JS
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>BLE Simulator Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, sans-serif;
            background: #1a1a2e;
            color: #e4e4e4;
            padding: 20px;
            max-width: 500px;
            margin: 0 auto;
        }
        h1 { margin-bottom: 10px; }
        .subtitle { color: #888; margin-bottom: 20px; }
        .card {
            background: #16213e;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .card h2 {
            font-size: 14px;
            color: #888;
            text-transform: uppercase;
            margin-bottom: 15px;
        }
        .status-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #0f3460;
        }
        .status-row:last-child { border: none; }
        .status-dot {
            width: 10px; height: 10px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 8px;
        }
        .online { background: #4ade80; }
        .offline { background: #f87171; }
        label { display: block; margin-bottom: 5px; color: #888; font-size: 14px; }
        input, select {
            width: 100%;
            padding: 12px;
            margin-bottom: 15px;
            background: #0f3460;
            border: none;
            border-radius: 8px;
            color: #e4e4e4;
            font-size: 16px;
        }
        input:focus { outline: 2px solid #4ade80; }
        button {
            width: 100%;
            padding: 15px;
            background: #4ade80;
            color: #1a1a2e;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
        }
        button:hover { background: #22c55e; }
        .btn-danger { background: #f87171; }
        .btn-danger:hover { background: #ef4444; }
        .msg { padding: 10px; border-radius: 8px; margin-bottom: 15px; }
        .msg-success { background: #064e3b; }
        .msg-error { background: #7f1d1d; }
    </style>
</head>
<body>
    <h1>BLE Simulator</h1>
    <p class="subtitle" id="apName">Loading...</p>

    <div class="card">
        <h2>Status</h2>
        <div class="status-row">
            <span>WiFi</span>
            <span><span class="status-dot" id="wifiDot"></span><span id="wifiStatus">-</span></span>
        </div>
        <div class="status-row">
            <span>MQTT</span>
            <span><span class="status-dot" id="mqttDot"></span><span id="mqttStatus">-</span></span>
        </div>
        <div class="status-row">
            <span>BLE</span>
            <span><span class="status-dot" id="bleDot"></span><span id="bleStatus">-</span></span>
        </div>
        <div class="status-row">
            <span>IP Address</span>
            <span id="ipAddr">-</span>
        </div>
    </div>

    <div class="card">
        <h2>WiFi Configuration</h2>
        <div id="message"></div>
        <form id="configForm">
            <label>WiFi Network Name (SSID)</label>
            <input type="text" name="ssid" id="ssid" required>

            <label>WiFi Password</label>
            <input type="password" name="password" id="password">

            <label>MQTT Server IP</label>
            <input type="text" name="mqtt_host" id="mqtt_host" placeholder="192.168.1.100" required>

            <label>MQTT Port</label>
            <input type="number" name="mqtt_port" id="mqtt_port" value="1883">

            <label>Device ID</label>
            <input type="text" name="device_id" id="device_id" placeholder="esp32-01">

            <button type="submit">Save & Connect</button>
        </form>
    </div>

    <div class="card">
        <button class="btn-danger" onclick="resetConfig()">Reset Configuration</button>
    </div>

    <script>
        let configLoaded = false;

        // Load config once on page load
        async function loadConfig() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();

                document.getElementById('apName').textContent = data.apName;
                document.getElementById('ssid').value = data.config.ssid || '';
                document.getElementById('mqtt_host').value = data.config.mqttHost || '';
                document.getElementById('mqtt_port').value = data.config.mqttPort || 1883;
                document.getElementById('device_id').value = data.config.deviceId || '';

                updateStatusDots(data.status);
                configLoaded = true;
            } catch (e) {
                console.error('Failed to load config:', e);
            }
        }

        // Update only status indicators (not form fields)
        async function updateStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();
                updateStatusDots(data.status);
            } catch (e) {
                console.error('Failed to update status:', e);
            }
        }

        function updateStatusDots(status) {
            document.getElementById('wifiDot').className = 'status-dot ' + (status.wifiConnected ? 'online' : 'offline');
            document.getElementById('wifiStatus').textContent = status.wifiConnected ? 'Connected' : 'Disconnected';

            document.getElementById('mqttDot').className = 'status-dot ' + (status.mqttConnected ? 'online' : 'offline');
            document.getElementById('mqttStatus').textContent = status.mqttConnected ? 'Connected' : 'Disconnected';

            document.getElementById('bleDot').className = 'status-dot ' + (status.bleStarted ? 'online' : 'offline');
            document.getElementById('bleStatus').textContent = status.bleStarted ? status.deviceType : 'Not started';

            document.getElementById('ipAddr').textContent = status.ipAddress || '-';
        }

        document.getElementById('configForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const form = e.target;
            const data = {
                ssid: form.ssid.value,
                password: form.password.value,
                mqtt_host: form.mqtt_host.value,
                mqtt_port: parseInt(form.mqtt_port.value),
                device_id: form.device_id.value
            };

            try {
                const res = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                const result = await res.json();

                document.getElementById('message').innerHTML =
                    '<div class="msg msg-success">Configuration saved! Reconnecting...</div>';

                setTimeout(updateStatus, 3000);
            } catch (e) {
                document.getElementById('message').innerHTML =
                    '<div class="msg msg-error">Failed to save configuration</div>';
            }
        });

        async function resetConfig() {
            if (!confirm('Reset all configuration?')) return;

            try {
                await fetch('/api/reset', { method: 'POST' });
                document.getElementById('message').innerHTML =
                    '<div class="msg msg-success">Configuration reset! Rebooting...</div>';
                setTimeout(() => location.reload(), 3000);
            } catch (e) {
                document.getElementById('message').innerHTML =
                    '<div class="msg msg-error">Failed to reset</div>';
            }
        }

        // Load config once, then only update status
        loadConfig();
        setInterval(updateStatus, 5000);
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleGetStatus() {
    JsonDocument doc;

    doc["apName"] = configManager.getAPName();

    JsonObject config = doc["config"].to<JsonObject>();
    config["ssid"] = configManager.getWifiSsid();
    config["mqttHost"] = configManager.getMqttHost();
    config["mqttPort"] = configManager.getMqttPort();
    config["deviceId"] = configManager.getDeviceId();

    JsonObject status = doc["status"].to<JsonObject>();
    status["wifiConnected"] = currentStatus.wifiConnected;
    status["mqttConnected"] = currentStatus.mqttConnected;
    status["bleStarted"] = currentStatus.bleStarted;
    status["deviceType"] = currentStatus.deviceType;
    status["ipAddress"] = currentStatus.ipAddress;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handlePostConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // Update configuration
    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";
    String mqttHost = doc["mqtt_host"] | "";
    uint16_t mqttPort = doc["mqtt_port"] | 1883;
    String deviceId = doc["device_id"] | "";

    configManager.setWifiCredentials(ssid, password);
    configManager.setMqttConfig(mqttHost, mqttPort);
    configManager.setDeviceId(deviceId);
    configManager.save();

    server.send(200, "application/json", "{\"success\":true}");

    // Trigger reconnect (will be handled in main loop)
    Serial.println("Configuration updated, reconnecting...");
}

void handleReset() {
    configManager.clear();
    server.send(200, "application/json", "{\"success\":true}");

    Serial.println("Configuration reset, rebooting...");
    delay(1000);
    ESP.restart();
}

void setupWebPortal() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.on("/api/reset", HTTP_POST, handleReset);

    server.begin();
    Serial.println("Web portal started on http://192.168.4.1");
}

void handleWebPortal() {
    server.handleClient();
}

void updatePortalStatus(const PortalStatus& status) {
    currentStatus = status;
}
