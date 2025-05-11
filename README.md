# SplashPad MQTT Valve Controller

An Arduino-based Ethernet valve controller for splash pad systems. Supports motion-activated operation, temperature thresholds, maintenance mode, structured MQTT telemetry, and Home Assistant auto-discovery.

---

## 🚀 Features

- Controls up to **12 water valves** via relay board
- **Motion-activated** 15-minute runtime sessions
- **Temperature threshold** logic (configurable via MQTT)
- **Random valve cycling** every 10–30 seconds
- Unified **JSON status** topic for all telemetry
- **Maintenance mode** to suspend operation remotely
- **Auto-reboot** every 24h + 1–6h randomized delay
- **MAC-based MQTT topic prefixing**
- Integrated with **Home Assistant** via MQTT Discovery
- Supports **Last Will & Testament** for offline status

---

## System Logic

### Initialization
1. **Hardware Setup**:
   - Initializes Ethernet, motion sensor, and valve pins.
   - Reads or generates a unique MAC address for MQTT topic prefixing.
2. **MQTT Setup**:
   - Connects to the MQTT broker using credentials.
   - Subscribes to relevant topics for control and telemetry.

### Main Loop
The `loop()` function continuously:
- Reconnects to the MQTT broker if disconnected.
- Processes incoming MQTT messages via `mqttCallback()`.
- Publishes periodic status updates.
- Handles motion detection and valve cycling logic.
- Monitors auto-restart conditions.

---

## MQTT Topics and JSON Payloads

### Subscribed Topics
| Topic              | Description                           |
|--------------------|---------------------------------------|
| `splashpad/<MAC>/controller/config`| Configures thresholds, limits, and operational settings (e.g., `enable`, `maintenance`, `mode`). |
| `splashpad/<MAC>/controller/reset` | Triggers a system reset.              |
| `splashpad/weather`| Provides weather data (JSON).         |

#### Example Payloads
- **`controller/config`**:
  ```json
  {
    "on_temp": 74.0,
    "max_valves": 3,
    "enable": "ON",
    "maintenance": "OFF",
    "mode": "AUTO"
  }
  ```
- **`splashpad/weather`**:
  ```json
  {
    "temp": 76.3,
    "lightning": 15.0
  }
  ```

### Published Topics
| Topic         | Payload  | Description               |
|---------------|----------|---------------------------|
| `controller/status` | JSON | System status and telemetry. |

#### Example Status Payload
```json
{
  "status": "ON",
  "enabled": true,
  "temperature": 76.3,
  "on_temp": 74.0,
  "valve_active": true,
  "valves": [1, 3],
  "max_valves": 3,
  "maintenance": false,
  "motion": "MOTION_DETECTED",
  "timer_remaining": 720,
  "lightning_distance": 15.0,
  "location": "Park Center"
}
```

---

## Operational Modes
The system supports the following modes:

- **MODE_AUTO**: Fully automatic operation based on motion and temperature.
- **MODE_OFF**: Disables all operations and turns off valves.
- **MODE_MAINTENANCE_ON**: Enables manual control of valves.
- **MODE_MAINTENANCE_OFF**: Disables manual control.
- **MODE_MAINTENANCE_FORCED_ON**: Forces valves to stay on in maintenance mode.

---

## Logic Flow

### Motion Detection
1. Detects motion using the motion sensor.
2. If motion is detected and conditions are met (e.g., temperature threshold, not in maintenance mode):
   - Activates valve cycling.
   - Publishes `ON` status.

### Valve Cycling
1. Randomly selects and activates a subset of valves.
2. Cycles valves every 10–30 seconds while motion is detected.
3. Turns off all valves if:
   - No motion is detected for 15 minutes.
   - Temperature drops below the threshold.
   - The system is disabled.

### Weather Integration
1. Subscribes to `splashpad/weather` for temperature and lightning data.
2. Updates internal state based on received data:
   - Disables the system if lightning is detected within 20 miles.
   - Publishes `LIGHTNING_SHUTOFF` status.

### Auto-Restart
1. Schedules a restart every 24 hours plus a random delay of 1–6 hours.
2. Before restarting:
   - Turns off all valves.
   - Publishes `RESTARTING` status.

---

## Installation
1. Flash the Arduino sketch to the board.
2. Connect the hardware components:
   - Relay pins: D3 to A1.
   - Motion sensor: D2.
   - Ethernet Shield: Network connection.
3. Power on the system and monitor MQTT topics for status updates.

---

## Troubleshooting
- **No MQTT Connection**: Check network settings and MQTT broker credentials.
- **Valves Not Activating**: Verify motion sensor and temperature threshold.
- **Frequent Restarts**: Check power supply and auto-restart logic.

---

## License
GPL-3.0 License
