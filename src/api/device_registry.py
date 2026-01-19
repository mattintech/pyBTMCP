"""In-memory device registry for tracking ESP32 devices."""

from datetime import datetime
from typing import Any


class DeviceRegistry:
    """Tracks connected ESP32 devices and their state."""

    def __init__(self):
        self._devices: dict[str, dict[str, Any]] = {}
        # Track recently deleted devices to prevent MQTT re-registration
        self._deleted_devices: set[str] = set()

    def register_device(self, device_id: str, info: dict | None = None) -> bool:
        """Register a new device or update existing.

        Returns False if device was recently deleted (won't be registered).
        """
        # Skip recently deleted devices - prevents stale MQTT messages from re-registering
        if device_id in self._deleted_devices:
            return False

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

        return True

    def update_device(self, device_id: str, updates: dict) -> bool:
        """Update device state.

        Returns False if device was recently deleted (won't be updated).
        """
        if device_id not in self._devices:
            if not self.register_device(device_id):
                return False  # Device was deleted, don't update

        for key, value in updates.items():
            if key == "values" and isinstance(value, dict):
                # Merge values - ensure values dict exists
                if not self._devices[device_id].get("values"):
                    self._devices[device_id]["values"] = {}
                self._devices[device_id]["values"].update(value)
            else:
                self._devices[device_id][key] = value

        self._devices[device_id]["last_seen"] = datetime.utcnow().isoformat()
        return True

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
        """Remove a device from the registry and mark as deleted."""
        self._devices.pop(device_id, None)
        # Track as deleted to prevent MQTT re-registration
        self._deleted_devices.add(device_id)

    def is_deleted(self, device_id: str) -> bool:
        """Check if a device was recently deleted."""
        return device_id in self._deleted_devices

    def clear_deleted(self, device_id: str):
        """Allow a deleted device to be re-registered (for manual re-add)."""
        self._deleted_devices.discard(device_id)

    def clear_all_deleted(self):
        """Clear all deleted device tracking (e.g., on server restart)."""
        self._deleted_devices.clear()


# Global registry instance
device_registry = DeviceRegistry()
