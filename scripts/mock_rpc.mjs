#!/usr/bin/env node
/*
 * mock_rpc.js - Local mock RPC server for UI development without a Shelly device.
 *
 * Simulates the following Mongoose OS / Wallbox RPC endpoints:
 *   GET  /rpc/Wallbox.GetInfo   - returns live device status (slowly drifting values)
 *   POST /rpc/Config.Set        - accepts wifi/mqtt config, logs it, returns {result: true}
 *   GET  /rpc/FS.List           - returns a small fake log file list
 *   GET  /rpc/Wallbox.Reboot    - logs the call, returns {result: true}
 *   GET  /rpc/Wallbox.Reset     - logs the call, returns {result: true}
 *   GET  /rpc/Wallbox.ResetWifi - logs the call, returns {result: true}
 *   GET  /rpc/Wallbox.ResetEnergy - logs the call, returns {result: true}
 *   GET  /rpc/Wallbox.SetRelay  - logs the call, returns {result: true}
 *   WS   /rpc                   - WebSocket push of Wallbox.GetInfo every 3s
 *
 * HTTP Digest auth is NOT enforced (for local dev convenience). The browser
 * will not see a 401 challenge, so it won't pop up a login dialog.
 *
 * Usage:
 *   node scripts/mock_rpc.js [--port 3001] [--charging] [--ev]
 *
 * Then start webpack dev server (with proxy):
 *   npm run mock
 */

import { createServer } from "node:http";
import { WebSocketServer } from "ws";
import { parseArgs } from "node:util";

const { values: args } = parseArgs({
  options: {
    port:     { type: "string", default: "3001" },
    charging: { type: "boolean", default: false },
    ev:       { type: "boolean", default: false },
    help:     { type: "boolean", short: "h", default: false },
  },
  allowPositionals: false,
});

if (args.help) {
  console.info([
    "mock_rpc.js - local Shelly RPC mock for UI dev",
    "",
    "  --port <n>   Port to listen on (default: 3001)",
    "  --charging   Start in charging state",
    "  --ev         Start with EV detected",
    "  -h, --help   Show this help",
  ].join("\n"));
  process.exit(0);
}

const PORT = parseInt(args.port, 10);

/* Mutable device state - modified by Config.Set / SetRelay / Reset calls */
const state = {
  id: "wallbox-mock",
  sn: "MOCK001",
  mac: "AA:BB:CC:DD:EE:FF",
  ip: "127.0.0.1",
  app: "Wallbox-Shelly1PM",
  version: "1.0.0-mock",
  fw_build: "20260903-000000",
  fw_ts: "2026-09-03T00:00:00Z",
  uptime: 0,
  charging: args.charging,
  ev: args.ev,
  power: args.charging ? 3450 : 0,
  voltage: 230,
  current: args.charging ? 15.0 : 0,
  energy: 1240,
  intensity: args.charging ? 15 : 0,
  temperature: 38.5,
  mqtt_connected: false,
  mqtt_server: "",
  wifi_ssid: "",
  wifi_ssid1: "",
};

/* Slowly drift numeric values to make the UI feel alive */
setInterval(() => {
  state.uptime += 1;
  if (state.charging) {
    state.power = 3450 + Math.round((Math.random() - 0.5) * 100);
    state.current = +(15 + (Math.random() - 0.5) * 0.5).toFixed(2);
    state.energy += state.power / 3600;
    state.intensity = Math.round(state.current);
    state.voltage = 230 + Math.round((Math.random() - 0.5) * 4);
  }
  state.temperature = +(38.5 + (Math.random() - 0.5) * 2).toFixed(1);
}, 1000);

const getInfo = () => ({ ...state, energy: +state.energy.toFixed(0) });

const FAKE_LOGS = [
  { name: "wallbox.log", size: 1024 },
  { name: "wallbox.1.log", size: 512 },
];

function handleRpc(method, params, res) {
  const lower = method.toLowerCase();

  if (lower === "wallbox.getinfo") {
    return respond(res, 200, getInfo());
  }

  if (lower === "config.set") {
    const cfg = params && params.config ? params.config : {};
    if (cfg.wifi) {
      if (cfg.wifi.sta)  state.wifi_ssid  = cfg.wifi.sta.ssid  || state.wifi_ssid;
      if (cfg.wifi.sta1) state.wifi_ssid1 = cfg.wifi.sta1.ssid || state.wifi_ssid1;
      console.info("[Config.Set] wifi:", JSON.stringify({ sta: cfg.wifi.sta, sta1: cfg.wifi.sta1 }));
    }
    if (cfg.mqtt) {
      if (cfg.mqtt.server !== undefined) state.mqtt_server = cfg.mqtt.server;
      if (cfg.mqtt.enable !== undefined) {
        state.mqtt_connected = !!cfg.mqtt.enable;
      }
      console.info("[Config.Set] mqtt:", JSON.stringify({ enable: cfg.mqtt.enable, server: cfg.mqtt.server, user: cfg.mqtt.user }));
    }
    return respond(res, 200, { result: true });
  }

  if (lower === "fs.list") {
    return respond(res, 200, { result: FAKE_LOGS });
  }

  if (lower === "wallbox.setrelay") {
    state.charging = !!(params && params.on);
    if (!state.charging) { state.power = 0; state.current = 0; state.intensity = 0; }
    console.info("[SetRelay] charging:", state.charging);
    return respond(res, 200, { result: true });
  }

  if (lower === "wallbox.resetenergy") {
    state.energy = 0;
    console.info("[ResetEnergy]");
    return respond(res, 200, { result: true });
  }

  if (lower === "wallbox.reboot" || lower === "wallbox.reset" || lower === "wallbox.resetwifi") {
    console.info("[" + method + "] (mock: no actual reboot)");
    return respond(res, 200, { result: true });
  }

  console.warn("[mock] Unknown method:", method);
  respond(res, 404, { error: { code: -1, message: "Method not found: " + method } });
}

function respond(res, code, body) {
  const json = JSON.stringify(body);
  res.writeHead(code, {
    "Content-Type": "application/json",
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  });
  res.end(json);
}

/* HTTP server */
const server = createServer((req, res) => {
  if (req.method === "OPTIONS") {
    res.writeHead(204, {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Headers": "Content-Type, Authorization",
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    });
    return res.end();
  }

  const url = new URL(req.url, "http://localhost");
  const pathname = url.pathname;

  if (!pathname.startsWith("/rpc/")) {
    res.writeHead(404);
    return res.end("Not found");
  }

  const method = pathname.slice(5); /* strip "/rpc/" */

  if (req.method === "GET") {
    const params = {};
    for (const [k, v] of url.searchParams) params[k] = v;
    return handleRpc(method, Object.keys(params).length ? params : null, res);
  }

  if (req.method === "POST") {
    let body = "";
    req.on("data", (chunk) => { body += chunk; });
    req.on("end", () => {
      let params = null;
      try { params = body ? JSON.parse(body) : null; } catch (_) { /* ignore */ }
      handleRpc(method, params, res);
    });
    return;
  }

  res.writeHead(405);
  res.end("Method not allowed");
});

/* WebSocket server (shares the same HTTP server) */
const wss = new WebSocketServer({ server, path: "/rpc" });

wss.on("connection", (ws) => {
  console.info("[WS] client connected");
  let timer = null;

  const push = () => {
    if (ws.readyState === ws.OPEN) {
      const payload = JSON.stringify({ id: ++wsId, result: getInfo() });
      ws.send(payload);
    }
  };

  ws.on("message", (raw) => {
    try {
      const msg = JSON.parse(raw.toString());
      const id = msg.id;
      const method = msg.method;
      /* Respond to Wallbox.GetInfo WS requests */
      if (method && method.toLowerCase() === "wallbox.getinfo") {
        ws.send(JSON.stringify({ id, result: getInfo() }));
        if (!timer) timer = setInterval(push, 3000);
      }
    } catch (_) { /* ignore */ }
  });

  ws.on("close", () => {
    console.info("[WS] client disconnected");
    if (timer) { clearInterval(timer); timer = null; }
  });
});

let wsId = 0;

server.listen(PORT, "127.0.0.1", () => {
  console.info("Mock RPC server running on http://127.0.0.1:" + PORT);
  console.info("Endpoints:");
  console.info("  GET  http://127.0.0.1:" + PORT + "/rpc/Wallbox.GetInfo");
  console.info("  POST http://127.0.0.1:" + PORT + "/rpc/Config.Set");
  console.info("  GET  http://127.0.0.1:" + PORT + "/rpc/FS.List");
  console.info("  WS   ws://127.0.0.1:"   + PORT + "/rpc");
  console.info("\nState: charging=" + state.charging + ", ev=" + state.ev);
  console.info("Pass --charging or --ev to start in those states.");
});

