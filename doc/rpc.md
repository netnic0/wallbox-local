# RPC

## APIs

### GetInfo

The GetInfo API returns configuration and state information about the wallbox.

```console
GET /rpc/Wallbox.GetInfo
```

```json
{
    "id": "wallbox-ABCDEF",
    "sn": "534C464346529AF4ABABCDEF",
    "app": "Wallbox-Shelly1PM",
    "version": "1.1.0",
    "fw_build": "20200601-000000",
    "fw_ts": "2020-06-01T00:00:00Z",
    "mac": "9AF4ABABCDEF",
    "ip": "192.168.1.123",
    "uptime": 15290,
    "temperature": 45.6,
    "wifi_ssid": "HomeWiFi",
    "wifi_ssid1": "HomeWiFi-Backup",
    "energy": 0,
    "intensity": 8,
    "state": false,
    "mqtt_state": false,
    "mqtt_server": "",
    "power": 3680,
    "voltage": 230,
    "current": 16.00,
    "charging": false
}
```

Attributes:

- id: wallbox identifier
- sn: wallbox serial number
- app: wallbox application identifier
- version: wallbox application version
- fw_build: firmware build number
- fw_ts: firmware timestamp
- mac: wallbox Wi-Fi MAC address
- ip: wallbox IP address on the local area network
- uptime: the number of seconds since the wallbox started, in seconds
- temperature: the internal temperature of the wallbox, in celsius
- wifi_ssid: SSID of the primary Wi-Fi network
- wifi_ssid1: SSID of the secondary (fallback) Wi-Fi network
- energy: the amount of energy for the current charging session, in Wh
- intensity: the configured charging current intensity, in A
- state: `true` if the relay is closed (charging session on-going), else `false`
- mqtt_state: `true` if MQTT publishing is enabled (`mqtt.enable`), else `false`
- mqtt_server: the URL of the MQTT broker the wallbox is configured to use
- power: the instantaneous active power measured by the meter, in W
- voltage: the instantaneous line voltage measured by the meter, in V
- current: the instantaneous line current measured by the meter, in A (2 decimals)
- charging: `true` if the relay is closed (reflects the relay GPIO state), else `false`

> Since v1.1.0 (L2-B), `GetInfo` returns 21 fields. The four live-metric fields
> `power`, `voltage`, `current` and `charging` were added so the Web UI can display
> live data over RPC without depending on an MQTT broker. The OCPP fields
> (`ocpp_url`, `ocpp_name`, `ocpp_state`) were removed with the OCPP layer in v1.0.0,
> and `mqtt_user` was never part of this response.

### Reboot

The Reboot API restarts the wallbox.

```console
POST /rpc/Wallbox.Reboot
```

```json
{}
```

### Reset

The Reset API resets the wallbox to factory settings by removing all
user-specific configuration and rebooting.

```console
POST /rpc/Wallbox.Reset
```

```json
{}
```

### ResetWifi

The ResetWifi API clears the Wi-Fi station configuration, re-enables the access
point (AP) mode and reboots so the wallbox can be provisioned again.

```console
POST /rpc/Wallbox.ResetWifi
```

```json
{}
```

### SetRelay

The SetRelay API opens or closes the relay that controls the charging session.
Turning the relay OFF ends the session and immediately persists the energy
counters. The relay state is not persisted across reboots (forced OFF at boot
for safety).

```console
POST /rpc/Wallbox.SetRelay
```

Request:

```json
{ "on": true }
```

Response:

```json
{ "relay": true }
```

- on: `true` to close the relay (start charging), `false` to open it (stop)
- relay: the resulting relay state

### ResetEnergy

The ResetEnergy API zeroes the meter energy counters for the current session.

```console
POST /rpc/Wallbox.ResetEnergy
```

```json
{}
```

## Methods summary

All RPC methods are open on the UART channel and require HTTP Digest
authentication (user `admin`, realm `wallbox`) on the HTTP, WebSocket and MQTT
channels. See the README "Security" section.

| Method                | Params        | HTTP/WS/MQTT auth | UART |
| --------------------- | ------------- | ----------------- | ---- |
| `Wallbox.GetInfo`     | none          | `+admin`          | open |
| `Wallbox.Reboot`      | none          | `+admin`          | open |
| `Wallbox.Reset`       | none          | `+admin`          | open |
| `Wallbox.ResetWifi`   | none          | `+admin`          | open |
| `Wallbox.SetRelay`    | `{on: bool}`  | `+admin`          | open |
| `Wallbox.ResetEnergy` | none          | `+admin`          | open |

