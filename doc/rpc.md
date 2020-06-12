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
    "version": "0.1.0",
    "fw_build": "20200601-000000",
    "fw_ts": "2020-06-01T00:00:00Z",
    "mac": "9AF4ABABCDEF",
    "ip": "192.168.1.123",
    "uptime": 15290,
    "wifi_ssid": "HomeWiFi",
    "energy": 0,
    "state": false,
    "ocpp_url": "wss://ocpp-server.example.com/OCPP/1234567890ABCDEF",
    "ocpp_name": "Station-01",
    "ocpp_state": true,
    "mqtt_state": false,
    "mqtt_server": "",
    "mqtt_user": ""
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
- wifi_ssid: SSID of the Wi-Fi network
- energy: the amount of energy for the current charging session, in Wh
- state: `true` if a charging session is on-going, else `false`
- ocpp_url: URL of the OCPP central system
- ocpp_name: the name of the charging station
- ocpp_state: `true` if the wallbox is connected to e-Mobility, else `false`
- mqtt_state: `true` if the wallbox is sending MQTT topics, else `false`
- mqtt_server: the URL the MQTT broker the wallbox is connected to
- mqtt_user: the user name for the MQTT broker

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
