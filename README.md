# Wallbox

Wallbox firmware for Shelly 1PM relay.

## Usage

See [Quick Start Guide](./doc/quick-start.md) to understand how to set up and use the Wallbox.

## Development

### Requirements

* [Mongoose OS](https://mongoose-os.com/docs/mongoose-os/quickstart/setup.md)
* [Node.js](https://nodejs.org/en/download/) (npm)
* curl

### Quick setup

Set MOS_PORT environment variable, eg. COM8, /dev/ttyS2 or <http://192.168.1.123/rpc.>

```sh
npm install
npm build
```

### Build

```sh
mos build --local --platform esp8266"
```

### Flash/Update

```sh
mos --port COM8 flash
```

```sh
curl -v -F file=@build/fw.zip http://192.168.10.30/update

```

### Device configuration

Examples:

```sh
mos --port http://192.168.1.123/rpc config-get
mos --port http://192.168.1.123/rpc config-get debug
mos --port http://192.168.1.123/rpc config-get wifi
mos --port http://192.168.1.123/rpc config-get wifi.ap
```

```sh
mos --port http://192.168.1.123/rpc config-set <key> <value>
```

#### Debug

Get debug configuration:

```sh
mos --port http://192.168.1.123/rpc config-get debug
```

#### Console via USB

```sh
mos --port COM8 console
```

#### Console via OTA

Set UDP log address to local machine:

```sh
mos --port ws://192.168.1.123/rpc config-set debug.udp_log_addr=192.168.1.100:1993
```

Start console on local machine:

```sh
mos --port udp://:1993/ console
```
