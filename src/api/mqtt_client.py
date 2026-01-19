"""MQTT client manager for communicating with ESP32 devices."""

import asyncio
import json
import os
from typing import Callable
import aiomqtt


class MQTTManager:
    """Manages MQTT connection and message handling."""

    def __init__(self):
        self.client: aiomqtt.Client | None = None
        self._connected = False
        self._message_handlers: dict[str, list[Callable]] = {}
        self._listen_task: asyncio.Task | None = None

    @property
    def is_connected(self) -> bool:
        return self._connected

    async def connect(self):
        """Connect to MQTT broker."""
        host = os.getenv("MQTT_HOST", "localhost")
        port = int(os.getenv("MQTT_PORT", "1883"))

        try:
            self.client = aiomqtt.Client(hostname=host, port=port)
            await self.client.__aenter__()
            self._connected = True

            # Subscribe to device status topics
            await self.client.subscribe("ble-sim/+/status")
            await self.client.subscribe("ble-sim/+/values")

            # Start listening for messages
            self._listen_task = asyncio.create_task(self._listen())

            print(f"MQTT connected to {host}:{port}")
        except Exception as e:
            print(f"MQTT connection failed: {e}")
            self._connected = False

    async def disconnect(self):
        """Disconnect from MQTT broker."""
        if self._listen_task:
            self._listen_task.cancel()
            try:
                await self._listen_task
            except asyncio.CancelledError:
                pass

        if self.client:
            await self.client.__aexit__(None, None, None)
            self._connected = False

    async def _listen(self):
        """Listen for incoming MQTT messages."""
        try:
            async for message in self.client.messages:
                topic = str(message.topic)
                payload = message.payload.decode()

                # Notify handlers
                for pattern, handlers in self._message_handlers.items():
                    if self._topic_matches(pattern, topic):
                        for handler in handlers:
                            try:
                                await handler(topic, payload)
                            except Exception as e:
                                print(f"Handler error: {e}")
        except asyncio.CancelledError:
            pass
        except aiomqtt.MqttError:
            # Expected during shutdown when client disconnects
            pass

    def _topic_matches(self, pattern: str, topic: str) -> bool:
        """Check if topic matches pattern (supports + and # wildcards)."""
        pattern_parts = pattern.split("/")
        topic_parts = topic.split("/")

        if len(pattern_parts) != len(topic_parts):
            if "#" not in pattern_parts:
                return False

        for p, t in zip(pattern_parts, topic_parts):
            if p == "#":
                return True
            if p != "+" and p != t:
                return False

        return True

    def on_message(self, topic_pattern: str):
        """Decorator to register a message handler."""
        def decorator(func: Callable):
            if topic_pattern not in self._message_handlers:
                self._message_handlers[topic_pattern] = []
            self._message_handlers[topic_pattern].append(func)
            return func
        return decorator

    async def publish(self, topic: str, payload: dict | str, retain: bool = False):
        """Publish a message to MQTT."""
        if not self._connected or not self.client:
            raise RuntimeError("MQTT not connected")

        if isinstance(payload, dict):
            payload = json.dumps(payload)

        await self.client.publish(topic, payload, retain=retain)

    async def clear_retained(self, topic: str):
        """Clear a retained message by publishing empty payload with retain flag."""
        if not self._connected or not self.client:
            return
        await self.client.publish(topic, "", retain=True)

    async def configure_device(self, device_id: str, device_type: str):
        """Send configuration command to a device."""
        await self.publish(
            f"ble-sim/{device_id}/config",
            {"type": device_type}
        )

    async def set_device_values(self, device_id: str, values: dict):
        """Send value update to a device."""
        await self.publish(
            f"ble-sim/{device_id}/set",
            values
        )

    async def disconnect_ble(self, device_id: str, duration_ms: int = 0, teardown: bool = False):
        """Trigger BLE disconnect on a device.

        Args:
            device_id: The ESP32 device ID
            duration_ms: How long to pause advertising after disconnect (0 = immediate resume)
            teardown: If True, fully tear down BLE stack (device disappears from scans)
        """
        payload = {}
        if duration_ms > 0:
            payload["duration_ms"] = duration_ms
        if teardown:
            payload["teardown"] = True
        await self.publish(f"ble-sim/{device_id}/disconnect", payload)


# Global MQTT manager instance
mqtt_manager = MQTTManager()
