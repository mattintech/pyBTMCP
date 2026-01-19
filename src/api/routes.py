"""API routes for device control."""

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel, Field

from .mqtt_client import mqtt_manager
from .device_registry import device_registry

router = APIRouter()


class DeviceConfig(BaseModel):
    """Device configuration request."""
    device_type: str  # "heart_rate", "treadmill", "bike"


class DeviceValues(BaseModel):
    """Device values update request."""
    heart_rate: int | None = Field(None, ge=30, le=220)  # Valid BPM range
    speed: float | None = Field(None, ge=0, le=50)  # km/h, max 50 km/h
    incline: float | None = Field(None, ge=-10, le=40)  # percent
    cadence: int | None = Field(None, ge=0, le=300)  # rpm
    power: int | None = Field(None, ge=0, le=2000)  # watts


@router.get("/devices")
async def list_devices():
    """List all connected devices."""
    return {
        "devices": device_registry.get_all_devices()
    }


@router.get("/devices/{device_id}")
async def get_device(device_id: str):
    """Get a specific device's status."""
    device = device_registry.get_device(device_id)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


@router.post("/devices/{device_id}/configure")
async def configure_device(device_id: str, config: DeviceConfig):
    """Configure a device's type."""
    if not mqtt_manager.is_connected:
        raise HTTPException(status_code=503, detail="MQTT not connected")

    await mqtt_manager.configure_device(device_id, config.device_type)

    # Update local registry
    device_registry.update_device(device_id, {"type": config.device_type})

    return {"status": "ok", "device_id": device_id, "type": config.device_type}


@router.post("/devices/{device_id}/values")
async def set_device_values(device_id: str, values: DeviceValues):
    """Update a device's simulated values."""
    if not mqtt_manager.is_connected:
        raise HTTPException(status_code=503, detail="MQTT not connected")

    values_dict = values.model_dump(exclude_none=True)
    await mqtt_manager.set_device_values(device_id, values_dict)

    # Update local registry
    device_registry.update_device(device_id, {"values": values_dict})

    return {"status": "ok", "device_id": device_id, "values": values_dict}
