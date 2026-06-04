<!-- markdownlint-disable MD013 -->

# MQTT contract — Wallbox-Local v1.0.0+

The wallbox **publishes** real-time telemetry on three topics and **subscribes** to one command topic. All payloads are JSON.

Topic prefix: `wallbox/<id>/...` where `<id>` is `device.id` from the config (default `wallbox-XXXXXX`).

## Topics published by the wallbox

### `wallbox/<id>/announce` — startup announcement

Upon startup (after MQTT connect), the wallbox publishes an identification message **once**.
QoS 0, retained.

```json
{
  "id": "wallbox-ABCDEF",
  "app": "Wallbox-Shelly1PM",
  "version": "1.0.0",
  "sn": "534C464346529AF4ABABCDEF",
  "fw": "20260604-150904",
  "mac": "9AF4ABABCDEF",
  "ip": "192.168.1.123"
}
```

| Attribute | Description |
|---|---|
| `id` | Wallbox identifier (matches MQTT topic prefix) |
| `app` | Application name (always `Wallbox-Shelly1PM` for OTA compat) |
| `version` | Firmware version (semver) |
| `sn` | Wallbox serial number |
| `fw` | Firmware build identifier (timestamp + git short SHA) |
| `mac` | Wi-Fi MAC address |
| `ip` | IP address on the LAN |

### `wallbox/<id>/state` — telemetry (every ~60s + on command)

Published every 60 seconds **and immediately** after any received command (start, stop, reset_energy).
QoS 0, not retained.

```json
{
  "uptime": 4740,
  "connected": true,
  "charging": true,
  "energy": 1900,
  "intensity": 8,
  "tid": 0,
  "temperature": 45.6,
  "power": 3240,
  "voltage": 230,
  "current": 14.20
}
```

| Attribute | Type | Unit | Description |
|---|---|---|---|
| `uptime` | int | seconds | Time since the wallbox booted |
| `connected` | bool | — | Always `true` (sentinel of presence — use HA `expire_after` to detect device offline) |
| `charging` | bool | — | `true` when the relay is closed (= a charging session is potentially in progress) |
| `energy` | int | Wh | Energy delivered in the current session (resets via `reset_energy` command or `Wallbox.ResetEnergy` RPC) |
| `intensity` | int | A | Average current computed from the energy delta over time at 230 V (approximation, not RMS) |
| `tid` | int | — | Always `0` (legacy field kept for backward compat with old HA dashboards) |
| `temperature` | float | °C | Internal temperature of the wallbox |
| `power` | int | W | Real-time active power read from the BL0937 sensor |
| `voltage` | int | V | Real-time RMS voltage from the BL0937 sensor |
| `current` | float | A | Real-time RMS current from the BL0937 sensor |

> Note: `intensity` (computed) and `current` (measured) are not the same. `current` is an instantaneous RMS reading; `intensity` is a 60s-average derived from energy. Use `current` for live UI, `intensity` for historical charts.

### `wallbox/<id>/system` — system health (every ~60s)

QoS 0, not retained.

```json
{
  "heapSize": 51880,
  "freeHeapSize": 38192,
  "minFreeHeapSize": 23384,
  "fsSize": 233681,
  "fsFreeSpace": 137046
}
```

| Attribute | Type | Unit | Description |
|---|---|---|---|
| `heapSize` | int | bytes | Total heap memory |
| `freeHeapSize` | int | bytes | Currently free heap |
| `minFreeHeapSize` | int | bytes | All-time minimum free heap (high-water mark of usage) |
| `fsSize` | int | bytes | SPIFFS total size |
| `fsFreeSpace` | int | bytes | SPIFFS free space |

## Topic subscribed by the wallbox

### `wallbox/<id>/cmd` — remote commands

Publish a JSON message with `{"action":"<name>"}` to control the wallbox remotely.
Subscribed at QoS 1 (at-least-once delivery, internal default of the Mongoose OS MQTT lib).

The wallbox automatically re-subscribes to this topic after every reconnection (no user action needed).

| `action` value | Effect |
|---|---|
| `"start"` | Closes the relay (charging ON). Equivalent to `Wallbox.SetRelay {on:true}` RPC. |
| `"stop"` | Opens the relay (charging OFF). Equivalent to `Wallbox.SetRelay {on:false}` RPC. |
| `"reset_energy"` | Zeroes all energy counters (HLW8012 internal + persisted `meter.*` config). Equivalent to `Wallbox.ResetEnergy` RPC. |

Examples:

```bash
# Start charging
mosquitto_pub -h <broker> -t wallbox/wallbox-ABCDEF/cmd -m '{"action":"start"}'

# Stop charging
mosquitto_pub -h <broker> -t wallbox/wallbox-ABCDEF/cmd -m '{"action":"stop"}'

# Reset energy counters
mosquitto_pub -h <broker> -t wallbox/wallbox-ABCDEF/cmd -m '{"action":"reset_energy"}'
```

After the action is applied, the wallbox immediately publishes an updated `state` message (no need to wait for the 60s tick), so HA receives instant feedback on the new `charging` state.

Invalid JSON or unknown action values are logged and dropped (no response is published).

### Security

The cmd topic is **not authenticated at the payload level**. Anyone who can publish to your MQTT broker can control the wallbox. Rely on broker-level credentials (`mqtt.user` and `mqtt.pass` in the config) and ACLs for security.

## Home Assistant integration (HA 2024+)

The example below uses the modern `mqtt:` top-level key syntax (HA 2024+). Replace `wallbox-ABCDEF` with your actual `device.id`.

Add this to your `configuration.yaml` (or include it as a package):

```yaml
mqtt:
  binary_sensor:
    - name: "Wallbox Charging"
      unique_id: wallbox_charging
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: battery_charging
      value_template: "{{ 'ON' if value_json.charging else 'OFF' }}"
      expire_after: 180

  sensor:
    - name: "Wallbox Power"
      unique_id: wallbox_power
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: power
      unit_of_measurement: "W"
      state_class: measurement
      value_template: "{{ value_json.power | int }}"
      expire_after: 180

    - name: "Wallbox Voltage"
      unique_id: wallbox_voltage
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: voltage
      unit_of_measurement: "V"
      state_class: measurement
      value_template: "{{ value_json.voltage | int }}"
      expire_after: 180

    - name: "Wallbox Current"
      unique_id: wallbox_current
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: current
      unit_of_measurement: "A"
      state_class: measurement
      value_template: "{{ value_json.current | float }}"
      expire_after: 180

    - name: "Wallbox Session Energy"
      unique_id: wallbox_session_energy
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: energy
      unit_of_measurement: "Wh"
      state_class: total_increasing
      value_template: "{{ value_json.energy | int }}"
      expire_after: 180

    - name: "Wallbox Intensity (avg)"
      unique_id: wallbox_intensity_avg
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: current
      unit_of_measurement: "A"
      state_class: measurement
      value_template: "{{ value_json.intensity | int }}"
      expire_after: 180

    - name: "Wallbox Temperature"
      unique_id: wallbox_temperature
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: temperature
      unit_of_measurement: "°C"
      state_class: measurement
      value_template: "{{ value_json.temperature | float }}"
      expire_after: 180

    - name: "Wallbox Uptime"
      unique_id: wallbox_uptime
      state_topic: "wallbox/wallbox-ABCDEF/state"
      device_class: duration
      unit_of_measurement: "s"
      value_template: "{{ value_json.uptime | int }}"
      expire_after: 180

  switch:
    - name: "Wallbox Charging"
      unique_id: wallbox_charging_switch
      state_topic: "wallbox/wallbox-ABCDEF/state"
      command_topic: "wallbox/wallbox-ABCDEF/cmd"
      value_template: "{{ value_json.charging | lower }}"
      state_on: "true"
      state_off: "false"
      payload_on: '{"action":"start"}'
      payload_off: '{"action":"stop"}'
      qos: 1
      retain: false
      optimistic: false

  button:
    - name: "Wallbox Reset Energy"
      unique_id: wallbox_reset_energy
      command_topic: "wallbox/wallbox-ABCDEF/cmd"
      payload_press: '{"action":"reset_energy"}'
      qos: 1
      retain: false
```

The `switch.wallbox_charging` entity is the primary control: toggling it publishes `{"action":"start"}` or `{"action":"stop"}` to the cmd topic. The switch's reported state comes from the `state` topic's `charging` field, so it will reflect the actual relay state (including manual `Wallbox.SetRelay` RPC calls or external triggers).

The `button.wallbox_reset_energy` lets you zero the session counters with one press from a Lovelace dashboard.

### Lovelace card example

```yaml
type: vertical-stack
cards:
  - type: entities
    title: Wallbox
    entities:
      - entity: switch.wallbox_charging
      - entity: sensor.wallbox_power
      - entity: sensor.wallbox_current
      - entity: sensor.wallbox_voltage
      - entity: sensor.wallbox_session_energy
      - entity: sensor.wallbox_temperature
      - entity: sensor.wallbox_uptime
      - entity: button.wallbox_reset_energy
  - type: gauge
    entity: sensor.wallbox_power
    min: 0
    max: 7400
    severity:
      green: 0
      yellow: 4000
      red: 6000
```
