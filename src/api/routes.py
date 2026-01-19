"""API routes for device control."""

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel, Field

from .mqtt_client import mqtt_manager
from .device_registry import device_registry
from .hr_variation import hr_variation_manager

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
    battery: int | None = Field(None, ge=0, le=100)  # battery percentage
    distance: int | None = Field(None, ge=0)  # distance in meters


class HRVariationConfig(BaseModel):
    """HR variation configuration."""
    enabled: bool
    target: int | None = Field(None, ge=30, le=220)


class BLEDisconnectRequest(BaseModel):
    """BLE disconnect request."""
    duration_ms: int = Field(0, ge=0, le=60000)  # Max 60 seconds
    teardown: bool = Field(False, description="Full BLE stack teardown (device disappears from scans)")


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


@router.delete("/devices/{device_id}")
async def delete_device(device_id: str):
    """Remove a device from the registry and clear its retained MQTT messages."""
    device = device_registry.get_device(device_id)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    # Stop any HR variation for this device
    await hr_variation_manager.disable(device_id)

    # Clear retained MQTT messages so device doesn't reappear on restart
    try:
        await mqtt_manager.clear_retained(f"ble-sim/{device_id}/status")
        await mqtt_manager.clear_retained(f"ble-sim/{device_id}/values")
    except Exception as e:
        # Log the error but continue with deletion
        print(f"Warning: Failed to clear MQTT retained messages for {device_id}: {e}")

    # Remove from registry (also marks as deleted to block re-registration)
    device_registry.remove_device(device_id)

    # Broadcast deletion to all WebSocket clients so they remove the device card
    try:
        from .main import ws_manager
        await ws_manager.broadcast({
            "type": "device_deleted",
            "device_id": device_id
        })
    except Exception as e:
        print(f"Warning: Failed to broadcast device deletion: {e}")

    return {"status": "ok", "device_id": device_id, "message": "Device removed"}


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


@router.post("/devices/{device_id}/disconnect")
async def disconnect_ble(device_id: str, request: BLEDisconnectRequest = BLEDisconnectRequest()):
    """Simulate a BLE disconnect.

    Args:
        device_id: The ESP32 device ID
        request: Optional duration_ms and teardown parameters
    """
    if not mqtt_manager.is_connected:
        raise HTTPException(status_code=503, detail="MQTT not connected")

    await mqtt_manager.disconnect_ble(device_id, request.duration_ms, request.teardown)

    return {
        "status": "ok",
        "device_id": device_id,
        "duration_ms": request.duration_ms,
        "teardown": request.teardown
    }


@router.get("/devices/{device_id}/hr-variation")
async def get_hr_variation(device_id: str):
    """Get HR variation state for a device."""
    return hr_variation_manager.get_state(device_id)


@router.post("/devices/{device_id}/hr-variation")
async def set_hr_variation(device_id: str, config: HRVariationConfig):
    """Enable or disable HR variation for a device."""
    if config.enabled:
        await hr_variation_manager.enable(device_id, config.target)
    else:
        await hr_variation_manager.disable(device_id)
        # If target provided, set it directly
        if config.target is not None:
            await hr_variation_manager.set_target(device_id, config.target)

    return {
        "status": "ok",
        "device_id": device_id,
        **hr_variation_manager.get_state(device_id)
    }


class HRTargetRequest(BaseModel):
    """HR target request."""
    target: int = Field(ge=30, le=220)


@router.post("/devices/{device_id}/hr-target")
async def set_hr_target(device_id: str, request: HRTargetRequest):
    """Set HR target (works with or without variation enabled)."""
    await hr_variation_manager.set_target(device_id, request.target)
    return {
        "status": "ok",
        "device_id": device_id,
        **hr_variation_manager.get_state(device_id)
    }
