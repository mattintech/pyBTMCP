"""In-memory device registry for tracking ESP32 devices."""

from datetime import datetime
from typing import Any


class DeviceRegistry:
    """Tracks connected ESP32 devices and their state."""

    def __init__(self):
        self._devices: dict[str, dict[str, Any]] = {}

    def register_device(self, device_id: str, info: dict | None = None):
        """Register a new device or update existing."""
        now = datetime.utcnow().isoformat()

        if device_id not in self._devices:
            self._devices[device_id] = {
                "id": device_id,
                "type": None,
                "values": {},
                "online": True,
                "first_seen": now,
                "last_seen": now,
            }
        else:
            self._devices[device_id]["last_seen"] = now
            self._devices[device_id]["online"] = True

        if info:
            self._devices[device_id].update(info)

    def update_device(self, device_id: str, updates: dict):
        """Update device state."""
        if device_id not in self._devices:
            self.register_device(device_id)

        for key, value in updates.items():
            if key == "values" and isinstance(value, dict):
                # Merge values - ensure values dict exists
                if not self._devices[device_id].get("values"):
                    self._devices[device_id]["values"] = {}
                self._devices[device_id]["values"].update(value)
            else:
                self._devices[device_id][key] = value

        self._devices[device_id]["last_seen"] = datetime.utcnow().isoformat()

    def mark_offline(self, device_id: str):
        """Mark a device as offline."""
        if device_id in self._devices:
            self._devices[device_id]["online"] = False

    def get_device(self, device_id: str) -> dict | None:
        """Get a device by ID."""
        return self._devices.get(device_id)

    def get_all_devices(self) -> list[dict]:
        """Get all devices."""
        return list(self._devices.values())

    def remove_device(self, device_id: str):
        """Remove a device from the registry."""
        self._devices.pop(device_id, None)


# Global registry instance
device_registry = DeviceRegistry()
