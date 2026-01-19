"""MCP Server for BLE device simulator control."""

import asyncio
import json
import os
import aiomqtt
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent


# Device state (shared with API via MQTT)
devices: dict[str, dict] = {}

# MQTT client for this server
mqtt_client: aiomqtt.Client | None = None


def get_mqtt_host() -> str:
    return os.getenv("MQTT_HOST", "localhost")


def get_mqtt_port() -> int:
    return int(os.getenv("MQTT_PORT", "1883"))


# Create MCP server
server = Server("ble-simulator")


@server.list_tools()
async def list_tools() -> list[Tool]:
    """List available MCP tools."""
    return [
        Tool(
            name="list_devices",
            description="List all connected ESP32 BLE simulator devices",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": [],
            },
        ),
        Tool(
            name="configure_device",
            description="Configure an ESP32 to simulate a specific BLE device type",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                    "device_type": {
                        "type": "string",
                        "enum": ["heart_rate", "treadmill", "bike"],
                        "description": "Type of BLE device to simulate",
                    },
                },
                "required": ["device_id", "device_type"],
            },
        ),
        Tool(
            name="set_heart_rate",
            description="Set the simulated heart rate value for a device configured as a heart rate monitor",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                    "bpm": {
                        "type": "integer",
                        "minimum": 30,
                        "maximum": 220,
                        "description": "Heart rate in beats per minute",
                    },
                },
                "required": ["device_id", "bpm"],
            },
        ),
        Tool(
            name="set_treadmill_values",
            description="Set simulated values for a device configured as a treadmill",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                    "speed": {
                        "type": "number",
                        "minimum": 0,
                        "maximum": 25,
                        "description": "Speed in km/h",
                    },
                    "incline": {
                        "type": "number",
                        "minimum": -5,
                        "maximum": 30,
                        "description": "Incline percentage",
                    },
                },
                "required": ["device_id"],
            },
        ),
        Tool(
            name="set_bike_values",
            description="Set simulated values for a device configured as a bike/cycling trainer",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                    "power": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 2000,
                        "description": "Power in watts",
                    },
                    "cadence": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 200,
                        "description": "Cadence in RPM",
                    },
                    "speed": {
                        "type": "number",
                        "minimum": 0,
                        "maximum": 80,
                        "description": "Speed in km/h",
                    },
                },
                "required": ["device_id"],
            },
        ),
        Tool(
            name="get_device_status",
            description="Get the current status and values of a specific device",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                },
                "required": ["device_id"],
            },
        ),
        Tool(
            name="simulate_ble_disconnect",
            description="Simulate a BLE client disconnection to test reconnection behavior",
            inputSchema={
                "type": "object",
                "properties": {
                    "device_id": {
                        "type": "string",
                        "description": "The ESP32 device ID",
                    },
                    "duration_ms": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 60000,
                        "default": 0,
                        "description": "How long to pause advertising after disconnect (0 = immediate resume)",
                    },
                    "teardown": {
                        "type": "boolean",
                        "default": False,
                        "description": "Full BLE stack teardown - device completely disappears from scans",
                    },
                },
                "required": ["device_id"],
            },
        ),
    ]


@server.call_tool()
async def call_tool(name: str, arguments: dict) -> list[TextContent]:
    """Handle tool calls."""
    global mqtt_client

    # Ensure MQTT is connected
    if mqtt_client is None:
        try:
            mqtt_client = aiomqtt.Client(
                hostname=get_mqtt_host(),
                port=get_mqtt_port()
            )
            await mqtt_client.__aenter__()
        except Exception as e:
            return [TextContent(
                type="text",
                text=f"Failed to connect to MQTT broker: {e}"
            )]

    try:
        if name == "list_devices":
            return [TextContent(
                type="text",
                text=json.dumps({
                    "devices": list(devices.values()),
                    "count": len(devices)
                }, indent=2)
            )]

        elif name == "configure_device":
            device_id = arguments["device_id"]
            device_type = arguments["device_type"]

            await mqtt_client.publish(
                f"ble-sim/{device_id}/config",
                json.dumps({"type": device_type})
            )

            # Update local state
            if device_id not in devices:
                devices[device_id] = {"id": device_id}
            devices[device_id]["type"] = device_type
            devices[device_id]["values"] = {}

            return [TextContent(
                type="text",
                text=f"Configured {device_id} as {device_type}"
            )]

        elif name == "set_heart_rate":
            device_id = arguments["device_id"]
            bpm = arguments["bpm"]

            # Validate BPM range
            if not (30 <= bpm <= 220):
                return [TextContent(
                    type="text",
                    text=f"Error: BPM must be between 30 and 220, got {bpm}"
                )]

            await mqtt_client.publish(
                f"ble-sim/{device_id}/set",
                json.dumps({"heart_rate": bpm})
            )

            if device_id in devices:
                devices[device_id].setdefault("values", {})["heart_rate"] = bpm

            return [TextContent(
                type="text",
                text=f"Set heart rate to {bpm} BPM on {device_id}"
            )]

        elif name == "set_treadmill_values":
            device_id = arguments["device_id"]
            values = {}
            if "speed" in arguments:
                values["speed"] = arguments["speed"]
            if "incline" in arguments:
                values["incline"] = arguments["incline"]

            await mqtt_client.publish(
                f"ble-sim/{device_id}/set",
                json.dumps(values)
            )

            if device_id in devices:
                devices[device_id].setdefault("values", {}).update(values)

            return [TextContent(
                type="text",
                text=f"Set treadmill values on {device_id}: {values}"
            )]

        elif name == "set_bike_values":
            device_id = arguments["device_id"]
            values = {}
            if "power" in arguments:
                values["power"] = arguments["power"]
            if "cadence" in arguments:
                values["cadence"] = arguments["cadence"]
            if "speed" in arguments:
                values["speed"] = arguments["speed"]

            await mqtt_client.publish(
                f"ble-sim/{device_id}/set",
                json.dumps(values)
            )

            if device_id in devices:
                devices[device_id].setdefault("values", {}).update(values)

            return [TextContent(
                type="text",
                text=f"Set bike values on {device_id}: {values}"
            )]

        elif name == "get_device_status":
            device_id = arguments["device_id"]
            device = devices.get(device_id)

            if device:
                return [TextContent(
                    type="text",
                    text=json.dumps(device, indent=2)
                )]
            else:
                return [TextContent(
                    type="text",
                    text=f"Device {device_id} not found"
                )]

        elif name == "simulate_ble_disconnect":
            device_id = arguments["device_id"]
            duration_ms = arguments.get("duration_ms", 0)
            teardown = arguments.get("teardown", False)

            payload = {}
            if duration_ms > 0:
                payload["duration_ms"] = duration_ms
            if teardown:
                payload["teardown"] = True

            await mqtt_client.publish(
                f"ble-sim/{device_id}/disconnect",
                json.dumps(payload)
            )

            if teardown:
                return [TextContent(
                    type="text",
                    text=f"BLE stack teardown on {device_id}, will reinit in {duration_ms if duration_ms > 0 else 3000}ms"
                )]
            elif duration_ms > 0:
                return [TextContent(
                    type="text",
                    text=f"Disconnected BLE on {device_id}, advertising paused for {duration_ms}ms"
                )]
            else:
                return [TextContent(
                    type="text",
                    text=f"Disconnected BLE on {device_id}, advertising resumed immediately"
                )]

        else:
            return [TextContent(
                type="text",
                text=f"Unknown tool: {name}"
            )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error: {e}"
        )]


async def handle_mqtt_messages():
    """Background task to handle incoming MQTT status messages."""
    global mqtt_client

    try:
        host = get_mqtt_host()
        port = get_mqtt_port()

        async with aiomqtt.Client(hostname=host, port=port) as client:
            await client.subscribe("ble-sim/+/status")
            await client.subscribe("ble-sim/+/values")

            async for message in client.messages:
                topic_parts = str(message.topic).split("/")
                if len(topic_parts) >= 3:
                    device_id = topic_parts[1]
                    msg_type = topic_parts[2]

                    try:
                        payload = json.loads(message.payload.decode())

                        if device_id not in devices:
                            devices[device_id] = {"id": device_id}

                        if msg_type == "status":
                            devices[device_id]["online"] = payload.get("online", True)
                            devices[device_id]["type"] = payload.get("type")
                        elif msg_type == "values":
                            devices[device_id].setdefault("values", {}).update(payload)
                    except json.JSONDecodeError:
                        pass
    except Exception as e:
        print(f"MQTT listener error: {e}", flush=True)


async def main():
    """Run the MCP server."""
    # Start MQTT listener in background
    mqtt_task = asyncio.create_task(handle_mqtt_messages())

    try:
        # Run MCP server on stdio
        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream,
                write_stream,
                server.create_initialization_options()
            )
    finally:
        mqtt_task.cancel()
        try:
            await mqtt_task
        except asyncio.CancelledError:
            pass


if __name__ == "__main__":
    asyncio.run(main())
