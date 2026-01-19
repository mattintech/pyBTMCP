#!/bin/bash
set -e

# Start Mosquitto MQTT broker in background
/usr/sbin/mosquitto -c /app/config/mosquitto.conf &
MOSQUITTO_PID=$!

# Wait for Mosquitto to be ready
sleep 1

# Start FastAPI/Web UI in background
python -m uvicorn src.api.main:app --host 0.0.0.0 --port 8000 &
UVICORN_PID=$!

# Trap to cleanup background processes on exit
cleanup() {
    kill $UVICORN_PID 2>/dev/null || true
    kill $MOSQUITTO_PID 2>/dev/null || true
}
trap cleanup EXIT

# Run MCP server as main process (stdio)
exec python -m src.mcp.server
