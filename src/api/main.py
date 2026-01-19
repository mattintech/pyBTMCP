"""FastAPI application for BLE device simulator control."""

import json
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

# Version info
__version__ = "1.0.0"
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
import os

from .routes import router
from .mqtt_client import mqtt_manager
from .device_registry import device_registry


class ConnectionManager:
    """Manages WebSocket connections for broadcasting device updates."""

    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        """Send message to all connected clients."""
        disconnected = []
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception:
                disconnected.append(connection)
        # Clean up disconnected clients
        for conn in disconnected:
            self.disconnect(conn)


ws_manager = ConnectionManager()


# Register MQTT message handlers
@mqtt_manager.on_message("ble-sim/+/status")
async def handle_device_status(topic: str, payload: str):
    """Handle device status updates from ESP32s."""
    parts = topic.split("/")
    if len(parts) >= 2:
        device_id = parts[1]
        try:
            data = json.loads(payload)
            device_registry.register_device(device_id)
            update_data = {
                "online": data.get("online", True),
                "type": data.get("type"),
                "ble_started": data.get("ble_started", False),
                "ip": data.get("ip"),
                "firmware_version": data.get("firmware_version"),
            }
            device_registry.update_device(device_id, update_data)
            print(f"Device {device_id} status updated: {data}")
            # Broadcast to WebSocket clients
            await ws_manager.broadcast({
                "type": "device_update",
                "device_id": device_id,
                "data": device_registry.get_device(device_id)
            })
        except json.JSONDecodeError:
            pass


@mqtt_manager.on_message("ble-sim/+/values")
async def handle_device_values(topic: str, payload: str):
    """Handle device value updates from ESP32s."""
    parts = topic.split("/")
    if len(parts) >= 2:
        device_id = parts[1]
        try:
            data = json.loads(payload)
            device_registry.update_device(device_id, {"values": data})
            print(f"Device {device_id} values updated: {data}")
            # Broadcast to WebSocket clients
            await ws_manager.broadcast({
                "type": "device_update",
                "device_id": device_id,
                "data": device_registry.get_device(device_id)
            })
        except json.JSONDecodeError:
            pass


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Manage application lifecycle - connect/disconnect MQTT."""
    await mqtt_manager.connect()
    yield
    await mqtt_manager.disconnect()


app = FastAPI(
    title="pyBTMCP",
    description="BLE Device Simulator Control API",
    version=__version__,
    lifespan=lifespan,
)

# CORS for web UI
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# API routes
app.include_router(router, prefix="/api")

# Serve static web UI
static_path = os.path.join(os.path.dirname(__file__), "..", "web", "static")
if os.path.exists(static_path):
    app.mount("/static", StaticFiles(directory=static_path), name="static")


@app.get("/")
async def root():
    """Serve web UI or redirect to API docs."""
    index_path = os.path.join(static_path, "index.html")
    if os.path.exists(index_path):
        return FileResponse(index_path)
    return {"message": "pyBTMCP API", "docs": "/docs"}


@app.get("/health")
async def health():
    """Health check endpoint."""
    return {
        "status": "healthy",
        "mqtt_connected": mqtt_manager.is_connected,
        "version": __version__,
    }


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time device updates."""
    await ws_manager.connect(websocket)
    try:
        # Send current device state on connect
        devices = device_registry.get_all_devices()
        await websocket.send_json({
            "type": "initial_state",
            "devices": devices
        })
        # Keep connection alive and handle any incoming messages
        while True:
            # Wait for messages (or connection close)
            await websocket.receive_text()
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception:
        ws_manager.disconnect(websocket)
