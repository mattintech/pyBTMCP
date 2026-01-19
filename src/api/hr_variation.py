"""Heart rate variation manager - smooth realistic HR simulation."""

import asyncio
import math
import random
from dataclasses import dataclass, field


@dataclass
class HRDeviceState:
    """State for a single device's HR variation."""
    target: int = 70
    float_hr: float = 70.0
    phase: float = field(default_factory=lambda: random.random() * math.pi * 2)
    enabled: bool = False
    task: asyncio.Task | None = field(default=None, repr=False)


class HRVariationManager:
    """Manages smooth HR variation for multiple devices."""

    def __init__(self):
        self._devices: dict[str, HRDeviceState] = {}
        self._mqtt_manager = None
        self._device_registry = None
        self._ws_broadcast = None

    def set_dependencies(self, mqtt_manager, device_registry, ws_broadcast=None):
        """Set required dependencies after initialization."""
        self._mqtt_manager = mqtt_manager
        self._device_registry = device_registry
        self._ws_broadcast = ws_broadcast

    def get_state(self, device_id: str) -> dict:
        """Get current HR variation state for a device."""
        if device_id not in self._devices:
            return {"enabled": False, "target": 70, "current": 70}

        state = self._devices[device_id]
        return {
            "enabled": state.enabled,
            "target": state.target,
            "current": round(state.float_hr),
        }

    async def set_target(self, device_id: str, target: int):
        """Set HR target for a device."""
        if device_id not in self._devices:
            self._devices[device_id] = HRDeviceState(target=target, float_hr=float(target))
        else:
            self._devices[device_id].target = target

        # If variation not enabled, send value directly
        if not self._devices[device_id].enabled:
            await self._send_hr(device_id, target)

    async def enable(self, device_id: str, target: int | None = None):
        """Enable HR variation for a device."""
        if device_id not in self._devices:
            initial_target = target or 70
            self._devices[device_id] = HRDeviceState(
                target=initial_target,
                float_hr=float(initial_target)
            )
        elif target is not None:
            self._devices[device_id].target = target

        state = self._devices[device_id]

        if state.enabled and state.task and not state.task.done():
            return  # Already running

        state.enabled = True
        state.task = asyncio.create_task(self._variation_loop(device_id))

    async def disable(self, device_id: str):
        """Disable HR variation for a device."""
        if device_id not in self._devices:
            return

        state = self._devices[device_id]
        state.enabled = False

        if state.task and not state.task.done():
            state.task.cancel()
            try:
                await state.task
            except asyncio.CancelledError:
                pass
            state.task = None

    async def stop_all(self):
        """Stop all variation tasks (for shutdown)."""
        for device_id in list(self._devices.keys()):
            await self.disable(device_id)

    async def _variation_loop(self, device_id: str):
        """Run smooth HR variation for a device."""
        state = self._devices[device_id]
        last_sent = None

        try:
            while state.enabled:
                # Advance phase (completes cycle in ~30 seconds)
                state.phase += 0.2

                # Smooth sinusoidal base variation (±2 BPM)
                sine_variation = math.sin(state.phase) * 2

                # Small random walk component
                random_walk = (random.random() - 0.5) * 0.6

                # Calculate ideal HR
                ideal_hr = state.target + sine_variation + random_walk

                # Smooth transition toward ideal
                # Use rate-limited step when far from target for smooth ramp
                distance = abs(ideal_hr - state.float_hr)
                if distance > 3:
                    # Far from target: move max 1 BPM per tick (2 BPM/sec at 0.5s ticks)
                    step = 1.0 if ideal_hr > state.float_hr else -1.0
                    state.float_hr += step
                else:
                    # Close to target: use proportional smoothing
                    state.float_hr += (ideal_hr - state.float_hr) * 0.2

                # Round for transmission
                current_hr = round(max(30, min(220, state.float_hr)))

                # Only send if changed
                if current_hr != last_sent:
                    last_sent = current_hr
                    await self._send_hr(device_id, current_hr)

                await asyncio.sleep(0.5)

        except asyncio.CancelledError:
            pass

    async def _send_hr(self, device_id: str, hr: int):
        """Send HR value to device via MQTT and update registry."""
        if self._mqtt_manager and self._mqtt_manager.is_connected:
            await self._mqtt_manager.publish(
                f"ble-sim/{device_id}/set",
                {"heart_rate": hr}
            )

        if self._device_registry:
            self._device_registry.update_device(device_id, {"values": {"heart_rate": hr}})

        # Broadcast to WebSocket clients
        if self._ws_broadcast:
            await self._ws_broadcast({
                "type": "hr_update",
                "device_id": device_id,
                "heart_rate": hr
            })


# Global instance
hr_variation_manager = HRVariationManager()
