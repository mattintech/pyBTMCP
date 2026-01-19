#include "services/web_service.h"
#include "services/config_service.h"
#include "services/wifi_service.h"
#include "device_state.h"
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);

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
        .btn-secondary { background: #6366f1; margin-top: 10px; }
        .btn-secondary:hover { background: #4f46e5; }
        .distance-value { font-size: 24px; font-weight: bold; color: #4ade80; }
        .battery-value { font-size: 24px; font-weight: bold; color: #4ade80; }
        .hidden { display: none; }
        input[type="range"] {
            -webkit-appearance: none;
            width: 100%;
            height: 8px;
            border-radius: 4px;
            background: #0f3460;
            margin: 10px 0;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4ade80;
            cursor: pointer;
        }
        .msg { padding: 10px; border-radius: 8px; margin-bottom: 15px; }
        .msg-success { background: #064e3b; }
        .msg-error { background: #7f1d1d; }
    </style>
</head>
<body>
    <h1>BLE Simulator</h1>
    <p class="subtitle"><span id="apName">Loading...</span> &bull; UI v1.2.0</p>

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

    <div class="card hidden" id="heartRateCard">
        <h2>Heart Rate Monitor</h2>
        <div class="status-row">
            <span>Battery Level</span>
            <span class="battery-value" id="batteryValue">100%</span>
        </div>
        <input type="range" id="batterySlider" min="0" max="100" value="100">
    </div>

    <div class="card hidden" id="treadmillCard">
        <h2>Treadmill</h2>
        <div class="status-row">
            <span>Distance</span>
            <span class="distance-value" id="distance">0 m</span>
        </div>
        <button class="btn-secondary" onclick="resetDistance()">Reset Distance</button>
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

        async function updateStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();
                updateStatusDots(data.status);
            } catch (e) {
                console.error('Failed to update status:', e);
                updateStatusDots({
                    wifiConnected: false,
                    mqttConnected: false,
                    bleStarted: false,
                    deviceType: 'Not configured',
                    ipAddress: '',
                    treadmillDistance: 0,
                    batteryLevel: 0
                });
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

            const heartRateCard = document.getElementById('heartRateCard');
            if (status.deviceType === 'Heart Rate') {
                heartRateCard.classList.remove('hidden');
                document.getElementById('batteryValue').textContent = status.batteryLevel + '%';
                document.getElementById('batterySlider').value = status.batteryLevel;
            } else {
                heartRateCard.classList.add('hidden');
            }

            const treadmillCard = document.getElementById('treadmillCard');
            if (status.deviceType === 'Treadmill') {
                treadmillCard.classList.remove('hidden');
                document.getElementById('distance').textContent = status.treadmillDistance + ' m';
            } else {
                treadmillCard.classList.add('hidden');
            }
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

        async function resetDistance() {
            try {
                await fetch('/api/reset-distance', { method: 'POST' });
                document.getElementById('distance').textContent = '0 m';
            } catch (e) {
                console.error('Failed to reset distance:', e);
            }
        }

        document.getElementById('batterySlider').addEventListener('input', async (e) => {
            const level = e.target.value;
            document.getElementById('batteryValue').textContent = level + '%';
            try {
                await fetch('/api/set-battery', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ level: parseInt(level) })
                });
            } catch (err) {
                console.error('Failed to set battery:', err);
            }
        });

        async function init() {
            let retries = 3;
            while (retries > 0) {
                try {
                    await loadConfig();
                    break;
                } catch (e) {
                    retries--;
                    if (retries > 0) await new Promise(r => setTimeout(r, 1000));
                }
            }
        }
        init();
        setInterval(updateStatus, 3000);
    </script>
</body>
</html>
)rawliteral";

WebService& WebService::getInstance() {
    static WebService instance;
    return instance;
}

void WebService::handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void WebService::handleGetStatus() {
    JsonDocument doc;

    doc["apName"] = configService.getAPName();

    JsonObject config = doc["config"].to<JsonObject>();
    config["ssid"] = configService.getWifiSsid();
    config["mqttHost"] = configService.getMqttHost();
    config["mqttPort"] = configService.getMqttPort();
    config["deviceId"] = configService.getDeviceId();

    const auto& connState = deviceState.getConnectionState();
    const auto& values = deviceState.getValues();

    JsonObject status = doc["status"].to<JsonObject>();
    status["wifiConnected"] = connState.wifiConnected;
    status["mqttConnected"] = connState.mqttConnected;
    status["bleStarted"] = deviceState.isBleStarted();
    status["deviceType"] = deviceState.getDeviceTypeString();
    status["ipAddress"] = connState.ipAddress;
    status["treadmillDistance"] = values.treadmillDistance;
    status["batteryLevel"] = values.batteryLevel;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebService::handlePostConfig() {
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

    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";
    String mqttHost = doc["mqtt_host"] | "";
    uint16_t mqttPort = doc["mqtt_port"] | 1883;
    String deviceId = doc["device_id"] | "";

    configService.setWifiCredentials(ssid, password);
    configService.setMqttConfig(mqttHost, mqttPort);
    configService.setDeviceId(deviceId);
    configService.save();

    server.send(200, "application/json", "{\"success\":true}");

    Serial.println("Configuration updated, reconnecting...");
    wifiService.reconnect();
}

void WebService::handleReset() {
    configService.clear();
    server.send(200, "application/json", "{\"success\":true}");

    Serial.println("Configuration reset, rebooting...");
    delay(1000);
    ESP.restart();
}

void WebService::handleResetDistance() {
    deviceState.resetTreadmillDistance();
    server.send(200, "application/json", "{\"success\":true}");
}

void WebService::handleSetBattery() {
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

    uint8_t level = doc["level"] | 100;
    if (level > 100) level = 100;

    deviceState.setBatteryLevel(level);
    Serial.print("Battery level set via web UI: ");
    Serial.println(level);

    server.send(200, "application/json", "{\"success\":true}");
}

void WebService::setup() {
    server.on("/", HTTP_GET, []() { webService.handleRoot(); });
    server.on("/api/status", HTTP_GET, []() { webService.handleGetStatus(); });
    server.on("/api/config", HTTP_POST, []() { webService.handlePostConfig(); });
    server.on("/api/reset", HTTP_POST, []() { webService.handleReset(); });
    server.on("/api/reset-distance", HTTP_POST, []() { webService.handleResetDistance(); });
    server.on("/api/set-battery", HTTP_POST, []() { webService.handleSetBattery(); });

    server.begin();
    Serial.println("Web portal started on http://192.168.4.1");
}

void WebService::loop() {
    server.handleClient();
}
