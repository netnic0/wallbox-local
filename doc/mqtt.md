<!-- markdownlint-disable MD013 -->
# MQTT

## Messages published

### Announcement

Upon startup, the wallbox sends a message on the announcement topic.
The message is sent once, but not guaranteed (QOS 0). The message is retained.

```json
{
    "id": "wallbox-ABCDEF",
    "app": "Wallbox-Shelly1PM",
    "version": "0.1.0",
    "sn": "534C464346529AF4ABABCDEF",
    "fw": "20200518-120000",
    "mac": "9AF4ABABCDEF",
    "ip": "192.168.1.123"
}
```

Attributes:

- id: wallbox identifier
- app: wallbox application identifier
- version: wallbox application version
- sn: wallbox serial number
- fw: firmware build number
- mac: wallbox Wi-Fi MAC address
- ip: wallbox IP address on the local area network

### State

Every 60 seconds, the wallbox sends a message on the state topic.
The message is sent once, but not guaranteed (QOS 0). The message is not retained.

```json
{
    "uptime": 4740,
    "power": 2709,
    "connected": true,
    "charging": true,
    "energy": 1900
}
```

Attributes:

- uptime: the number of seconds since the wallbox started, in seconds
- power: the current active power, in Watts
- connected: `true` if the wallbox is connected to e-Mobility, else `false`
- charging: `true` if a charging session is on-going, else `false`
- energy: the amount of energy for the current charging session, in Wh

### System

Every 60 seconds, the wallbox sends a message on the system topic.
The message is sent once, but not guaranteed (QOS 0). The message is not retained.

```json
{
    "heapSize": 51880,
    "freeHeapSize": 38192,
    "minFreeHeapSize": 23384,
    "fsSize": 233681,
    "fsFreeSpace": 137046
}
```

Attributes:

- heapSize: the system memory size, in bytes
- freeHeapSize: the system free memory, in bytes
- minFreeHeapSize: the minimal watermark of the system free memory, in bytes
- fsSize: the size of the file system, in bytes
- fsFreeSpace: the free space of the file system, in bytes

## Home Assistant

You can register sensors in Home Assistant.

Example `wallbox.yaml` package:

```yaml
homeassistant:
  customize:
    binary_sensor.wallbox_connected:
      friendly_name: Connexion e-Mobility
    binary_sensor.wallbox_charging:
      friendly_name: En charge
    sensor.wallbox_power:
      friendly_name: Puissance
    sensor.wallbox_energy:
      friendly_name: Énergie délivrée
    sensor.wallbox_uptime:
      friendly_name: Up time
    sensor.wallbox_heap_size:
      friendly_name: Mémoire totale
    sensor.wallbox_free_heap_size:
      friendly_name: Mémoire libre
    sensor.wallbox_min_free_heap_size:
      friendly_name: Mémoire libre minimale
    sensor.wallbox_fs_size:
      friendly_name: Système de fichier
    sensor.wallbox_fs_free_space:
      friendly_name: Espace libre

binary_sensor:
  - platform: mqtt
    name: "Wallbox Connected"
    device_class: plug
    state_topic: "wallbox/wallbox-ABCDEF/state"
    payload_off: false
    payload_on: true
    value_template: "{{ value_json.connected }}"
  - platform: mqtt
    name: "Wallbox Charging"
    device_class: battery_charging
    state_topic: "wallbox/wallbox-ABCDEF/state"
    payload_off: false
    payload_on: true
    value_template: "{{ value_json.charging }}"

sensor:
  - platform: mqtt
    name: "Wallbox Uptime"
    unit_of_measurement: 's'
    icon: mdi:clock
    state_topic: "wallbox/wallbox-ABCDEF/state"
    value_template: "{{ value_json.uptime | int }}"
  - platform: mqtt
    name: "Wallbox Power"
    unit_of_measurement: 'W'
    device_class: power
    icon: mdi:flash
    state_topic: "wallbox/wallbox-ABCDEF/state"
    value_template: "{{ value_json.power | int }}"
  - platform: mqtt
    name: "Wallbox Energy"
    unit_of_measurement: 'Wh'
    icon: mdi:flash
    state_topic: "wallbox/wallbox-ABCDEF/state"
    value_template: "{{ value_json.energy | int }}"
  - platform: mqtt
    name: "Wallbox Heap Size"
    unit_of_measurement: 'kb'
    icon: mdi:memory
    state_topic: "wallbox/wallbox-ABCDEF/system"
    value_template: "{{ ((value_json.heapSize | int) / 1000) | round(1) }}"
  - platform: mqtt
    name: "Wallbox Free Heap Size"
    unit_of_measurement: 'kb'
    icon: mdi:memory
    state_topic: "wallbox/wallbox-ABCDEF/system"
    value_template: "{{ ((value_json.freeHeapSize | int) / 1000) | round(1) }}"
  - platform: mqtt
    name: "Wallbox Min Free Heap Size"
    unit_of_measurement: 'kb'
    icon: mdi:memory
    state_topic: "wallbox/wallbox-ABCDEF/system"
    value_template: "{{ ((value_json.minFreeHeapSize | int) / 1000) | round(1) }}"
  - platform: mqtt
    name: "Wallbox FS Size"
    unit_of_measurement: 'kb'
    icon: mdi:harddisk
    state_topic: "wallbox/wallbox-ABCDEF/system"
    value_template: "{{ ((value_json.fsSize | int) / 1000) | round(1) }}"
  - platform: mqtt
    name: "Wallbox FS Free Space"
    unit_of_measurement: 'kb'
    icon: mdi:harddisk
    state_topic: "wallbox/wallbox-ABCDEF/system"
    value_template: "{{ ((value_json.fsFreeSpace | int) / 1000) | round(1) }}"

  - platform: template
    sensors:
      wallbox_uptime_readable:
        friendly_name: "Up time"
        icon_template: mdi:clock-outline
        value_template: >-
          {% set seconds = states.sensor.wallbox_uptime.state | int %}
          {% set days = ((seconds / (24 * 3600)) | int) %}
          {% set seconds = seconds % (24 * 3600) %}
          {% set hours = ((seconds / 3600) | int) %}
          {% set seconds = seconds % 3600 %}
          {% set minutes = ((seconds / 60) | int) %}
          {% set seconds = seconds % 60 %}
          {% if days > 0 %}{{ days }}d {% endif %}{% if hours > 0 %}{{ hours }}h {% endif %}{% if minutes > 0 %}{{ minutes }}m {% endif %}

rest_command:
  wallbox_reboot:
    url: "http://192.168.1.123/rpc/Wallbox.Reboot"
    method: post

recorder:
  exclude:
    entities:
      - sensor.wallbox_uptime
      - sensor.wallbox_uptime_readable

history:
  exclude:
    entities:
      - sensor.wallbox_uptime
      - sensor.wallbox_uptime_readable
```
