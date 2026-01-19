# pyBTMCP - BLE Device Simulator with MCP Integration

A comprehensive BLE (Bluetooth Low Energy) fitness device simulator that enables AI agents like Claude to control simulated heart rate monitors, treadmills, and cycling trainers via the Model Context Protocol (MCP).

## Overview

pyBTMCP consists of three integrated components:

1. **ESP32 Firmware** - Runs on ESP32 microcontrollers to simulate BLE fitness devices
2. **Python Backend** - FastAPI server with MQTT broker for device communication
3. **MCP Server** - Enables Claude and other AI agents to control devices

```
┌─────────────────┐     ┌──────────────────────────────────────────┐
│  Claude/AI      │     │           Docker Container               │
│  (MCP Client)   │◄───►│  ┌─────────┐  ┌───────┐  ┌──────────┐   │
└─────────────────┘     │  │   MCP   │  │ MQTT  │  │ FastAPI  │   │
                        │  │ Server  │◄►│Broker │◄►│  + Web   │   │
                        │  └─────────┘  └───┬───┘  └──────────┘   │
                        └──────────────────│───────────────────────┘
                                           │
                        ┌──────────────────▼───────────────────────┐
                        │              ESP32 Devices               │
                        │  ┌─────────┐ ┌─────────┐ ┌─────────┐    │
                        │  │   HR    │ │Treadmill│ │  Bike   │    │
                        │  │ Monitor │ │         │ │ Trainer │    │
                        │  └─────────┘ └─────────┘ └─────────┘    │
                        └──────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- Docker
- (Optional) ESP32 development board for hardware simulation

### 1. Build the Docker Image

```bash
docker build -t pybtmcp:latest .
```

### 2. Run the Container

```bash
docker run -it --rm \
  -p 1883:1883 \
  -p 8000:8000 \
  --name pybtmcp \
  pybtmcp:latest
```

This starts:
- **MQTT Broker** on port 1883 (for ESP32 device connections)
- **Web UI / API** on port 8000 (browser interface)
- **MCP Server** on stdio (for Claude integration)

### 3. Access the Web UI

Open http://localhost:8000 in your browser to see connected devices and control them manually.

## MCP Integration (Claude Desktop)

To use pyBTMCP with Claude Desktop, add it to your MCP configuration:

### Configuration File Location

- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`
- **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`

### Add the Server Configuration

```json
{
  "mcpServers": {
    "ble-simulator": {
      "command": "docker",
      "args": [
        "run", "-i", "--rm",
        "-p", "1883:1883",
        "-p", "8000:8000",
        "--name", "pybtmcp",
        "pybtmcp:latest"
      ]
    }
  }
}
```

### Restart Claude Desktop

After saving the configuration, restart Claude Desktop. You should see the "ble-simulator" MCP server available.

## MCP Integration (Claude Code CLI)

To add pyBTMCP to Claude Code, run:

```bash
claude mcp add ble-simulator \
  -s user \
  -- docker run -i --rm -p 1883:1883 -p 8000:8000 --name pybtmcp pybtmcp:latest
```

This adds the MCP server to your user configuration. Use `-s project` instead to add it to the current project only.

To verify the server was added:

```bash
claude mcp list
```

## MCP Tools Reference

Once configured, Claude can use these tools to control BLE devices:

### `list_devices`
List all connected ESP32 BLE simulator devices.

```
No parameters required
```

### `configure_device`
Configure an ESP32 to simulate a specific BLE device type.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| device_id | string | Yes | The ESP32 device ID |
| device_type | string | Yes | One of: `heart_rate`, `treadmill`, `bike` |

### `set_heart_rate`
Set the simulated heart rate for a heart rate monitor device.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| device_id | string | Yes | The ESP32 device ID |
| bpm | integer | Yes | Heart rate (30-220 BPM) |

### `set_treadmill_values`
Set simulated values for a treadmill device.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| device_id | string | Yes | The ESP32 device ID |
| speed | number | No | Speed in km/h (0-25) |
| incline | number | No | Incline percentage (-5 to 30) |

### `set_bike_values`
Set simulated values for a bike/cycling trainer device.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| device_id | string | Yes | The ESP32 device ID |
| power | integer | No | Power in watts (0-2000) |
| cadence | integer | No | Cadence in RPM (0-200) |
| speed | number | No | Speed in km/h (0-80) |

### `get_device_status`
Get the current status and values of a specific device.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| device_id | string | Yes | The ESP32 device ID |

## Web UI Features

The web interface at http://localhost:8000 provides:

- **Real-time device status** via WebSocket (live updates)
- **Device type configuration** (Heart Rate, Treadmill, Bike)
- **Preset values** for quick testing (Rest, Warm Up, Cardio, etc.)
- **Manual sliders** for fine-grained control
- **Unit switching** (Metric/Imperial)
- **HR variation** toggle for realistic heart rate simulation

### Connection Status Indicator

The UI shows WebSocket connection status:
- 🟢 **Live** - Connected and receiving real-time updates
- 🟡 **Connecting...** - Attempting to connect
- 🔴 **Disconnected** - Using polling fallback

## REST API

The FastAPI backend provides these endpoints:

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/devices` | List all devices |
| GET | `/api/devices/{id}` | Get device by ID |
| POST | `/api/devices/{id}/configure` | Configure device type |
| POST | `/api/devices/{id}/values` | Set device values |
| GET | `/health` | Health check |
| WS | `/ws` | WebSocket for real-time updates |

API documentation is available at http://localhost:8000/docs (Swagger UI).

## ESP32 Firmware Setup

### Hardware Requirements

- ESP32 development board (ESP32-DevKitC, ESP32-WROOM, etc.)
- USB cable for programming

### Building the Firmware

1. Install [PlatformIO](https://platformio.org/)
2. Navigate to the firmware directory:
   ```bash
   cd firmware/esp32_ble_sim
   ```
3. Build and upload:
   ```bash
   pio run -t upload
   ```

### Initial Configuration

1. On first boot, the ESP32 creates a WiFi access point: `BLE-Sim-XXXX`
2. Connect to the AP and open http://192.168.4.1
3. Enter your WiFi credentials and MQTT broker address
4. The device will restart and connect to your network

### MQTT Topics

The ESP32 devices communicate via MQTT:

| Topic | Direction | Description |
|-------|-----------|-------------|
| `ble-sim/{id}/status` | Device → Server | Device online status |
| `ble-sim/{id}/values` | Device → Server | Current sensor values |
| `ble-sim/{id}/config` | Server → Device | Device type configuration |
| `ble-sim/{id}/set` | Server → Device | Set sensor values |

## Development

### Running Locally (without Docker)

1. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

2. Start an MQTT broker (e.g., Mosquitto):
   ```bash
   mosquitto -c config/mosquitto.conf
   ```

3. Start the API server:
   ```bash
   uvicorn src.api.main:app --reload --port 8000
   ```

4. Run the MCP server (for testing):
   ```bash
   python -m src.mcp.server
   ```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| MQTT_HOST | localhost | MQTT broker hostname |
| MQTT_PORT | 1883 | MQTT broker port |

## Project Structure

```
pyBTMCP/
├── src/
│   ├── mcp/
│   │   └── server.py          # MCP server implementation
│   ├── api/
│   │   ├── main.py            # FastAPI app + WebSocket
│   │   ├── routes.py          # REST API routes
│   │   ├── mqtt_client.py     # MQTT connection manager
│   │   └── device_registry.py # Device state tracking
│   └── web/
│       └── static/
│           └── index.html     # Web UI
├── firmware/
│   └── esp32_ble_sim/         # PlatformIO ESP32 project
├── config/
│   └── mosquitto.conf         # MQTT broker config
├── Dockerfile
├── entrypoint.sh
├── requirements.txt
└── mcp-config.example.json    # Example Claude Desktop config
```

## Supported BLE Services

| Device Type | BLE Service | Characteristics |
|-------------|-------------|-----------------|
| Heart Rate Monitor | Heart Rate Service (0x180D) | Heart Rate Measurement |
| Treadmill | Fitness Machine Service (0x1826) | Treadmill Data |
| Bike Trainer | Cycling Power Service (0x1818) | Cycling Power Measurement |

## Troubleshooting

### MCP server not appearing in Claude Desktop

1. Ensure Docker is running
2. Check the config file path is correct for your OS
3. Restart Claude Desktop after saving config
4. Check Docker logs: `docker logs pybtmcp`

### ESP32 not connecting

1. Verify WiFi credentials are correct
2. Ensure MQTT broker is reachable from ESP32's network
3. Check MQTT broker is listening on port 1883
4. Use `mosquitto_sub -t "ble-sim/#" -v` to monitor MQTT traffic

### Web UI not updating

1. Check browser console for WebSocket errors
2. Verify the container is running: `docker ps`
3. The UI falls back to 5-second polling if WebSocket disconnects

## License

MIT License
