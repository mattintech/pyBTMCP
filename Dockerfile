FROM python:3.12-slim

# Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    mosquitto \
    mosquitto-clients \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy requirements first for layer caching
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY src/ ./src/
COPY config/ ./config/
COPY entrypoint.sh .

RUN chmod +x entrypoint.sh

# Expose ports
# 1883 - MQTT broker (for ESP32 connections)
# 8000 - Web UI / REST API
EXPOSE 1883 8000

# MCP server runs as main process via entrypoint
# Mosquitto and API run as background processes
ENTRYPOINT ["/app/entrypoint.sh"]
