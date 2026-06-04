# Wallbox-Local

Local-only wallbox firmware for **Shelly 1PM Gen1** (ESP8266 + BL0937), designed for direct integration with Home Assistant via MQTT — no cloud, no OCPP backend.

> **Fork notice** — This project is a fork of [sebastien-savalle/shelly-ocpp-wallbox](https://github.com/sebastien-savalle/shelly-ocpp-wallbox), originally authored by SAP Labs France & d-shop Caen and licensed under Apache 2.0. Forked at upstream commit `58c3691`. The OCPP / Open e-Mobility integration has been removed; the firmware now targets a fully local Home Assistant deployment. Original copyright notices are preserved in the source headers and the [LICENSE](./LICENSE) file is unchanged. See [NOTICE](./NOTICE) for the formal attribution.

## Features

- 🏠 Native Home Assistant integration via MQTT (compatible with the original `wallbox/<id>/state` topic contract)
- 📡 Real-time current/voltage/power readings via HLW8012 (BL0937)
- 🔌 Manual relay control via RPC (`Wallbox.SetRelay`)
- ⚡ Energy session + total counters (`Wallbox.ResetEnergy`)
- 🚗 EV charge detection (planned for v1.2.0)
- 🛡️ Thermal & overcurrent protection (planned for v1.2.0)

## Versions

| Version | Status | Highlights |
|---|---|---|
| v1.0.0 | 🚧 in progress | Backend cleanup, OCPP removal, MQTT enrichment |
| v1.1.0 | 📝 planned | Modern vanilla-JS Web UI |
| v1.2.0 | 📝 planned | HA MQTT Discovery, EV charge detection, safety features |

See [PLAN.md](./PLAN.md) and [docs/PLAN-L1.md](./docs/PLAN-L1.md) for the implementation roadmap.

## Documentation

- 📖 [RPC APIs](./doc/rpc.md) — administration APIs
- 📖 [MQTT contract](./doc/mqtt.md) — topics and Home Assistant integration
- 📖 [Quick Start Guide](./doc/quick-start.md) — initial setup

## Development

### Requirements

- WSL2 / Linux with Docker
- Node.js >= 18 (for the Web UI build)

### Build (local, via Docker — Mongoose OS server-side build is no longer available)

```sh
# 1. Install npm deps and build the Web UI
npm install
npm run webpack

# 2. Build the firmware via Docker
docker run --rm \
  --entrypoint /bin/sh \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v "$PWD:$PWD" -w "$PWD" \
  mgos/mos:latest \
  -c 'git config --global --add safe.directory "*" && mos build --local --platform esp8266 --verbose'

# Output: build/fw.zip
```

### Flash / Update

OTA via the existing Web UI (recommended for an already-flashed Shelly):

```sh
curl -v -F file=@build/fw.zip http://<wallbox-ip>/update
```

Initial flash via serial (USB-to-TTL adapter required for a stock Shelly):

```sh
mos --port COM8 flash
```

### Device configuration

```sh
mos --port http://<wallbox-ip>/rpc config-get
mos --port http://<wallbox-ip>/rpc config-set <key> <value>
```

### Console

Via OTA (UDP log):

```sh
mos --port ws://<wallbox-ip>/rpc config-set debug.udp_log_addr=<your-host>:1993
mos --port udp://:1993/ console
```

## License

Apache 2.0 — see [LICENSE](./LICENSE) and [NOTICE](./NOTICE).
