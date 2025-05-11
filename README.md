
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

## 🧩 MQTT Topics

All topics use this format:

```
splashpad/<MAC>/controller/<subtopic>
```

### Subscribed Topics

| Topic              | Description                           |
|--------------------|---------------------------------------|
| `enable`           | `"ON"` or `"OFF"` to activate system  |
| `weather/temp`     | Temperature input (float °F)          |
| `config`           | JSON config: `on_temp`, `max_valves` |
| `maintenance`      | `"ON"` disables valve cycling         |
| `reset`            | Any payload triggers reboot           |

### Published Topic

| Topic         | Payload  | Description               |
|---------------|----------|---------------------------|
| `status`      | JSON     | All state in one object   |

Example:
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
  "timer_remaining": 720
}
```

---

## ⚙️ Configuration via MQTT

You can send a JSON payload to `controller/config`:
```json
{
  "on_temp": 74.0,
  "max_valves": 3
}
```

---

## 🏠 Home Assistant Support

This controller uses MQTT Discovery to register:
- Temperature sensor
- Timer remaining
- Motion sensor
- Enable switch

No manual YAML needed.

---

## 🔁 Auto-Restart Logic

The controller resets itself every 24 hours + 1–6 hour randomized buffer to prevent lockups. It safely turns off valves and publishes a `"RESTARTING"` status before doing so.

---

## 📦 Installing

1. Flash the Arduino sketch (`.ino`) to an Uno or Mega
2. Connect:
   - Relay pins: D3 to A1
   - Motion sensor to D2
   - Ethernet Shield to your network
3. Subscribe to MQTT topic to confirm startup

---

## 📜 License

MIT License
