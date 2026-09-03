#!/usr/bin/env node
/*eslint no-console: "off"*/
/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

/**
 * build_fw.mjs - build a flashable firmware bundle with baked-in configuration.
 *
 * Runs the standard web + firmware build to produce build/fw.zip, then appends
 * a user-layer configuration partition (conf9, the highest-priority layer
 * applied at boot) using mos create-fw-bundle. The resulting build/fw_conf.zip
 * comes up already provisioned with the Wi-Fi / MQTT / device settings.
 *
 * The configuration is added as a SEPARATE conf partition in the bundle
 * manifest; the SPIFFS image (fs.bin) is NOT modified. Layer conf9 wins over
 * the schema defaults (conf0) and every vendor layer.
 *
 * IMPORTANT - this partition is wiped by ANY factory reset (6-reboot hard reset
 * or the Wallbox.Reset RPC). Recovery after a reset is to re-flash a bundle
 * produced by this script. See docs/BUILD-AND-FLASH.md.
 *
 * SECURITY - the produced build/fw_conf.zip and the intermediate
 * build/conf9.json contain plaintext credentials. They are gitignored and MUST
 * NOT be committed or shared. build/conf9.json is deleted after bundling unless
 * --keep-conf is passed.
 *
 * Wi-Fi slot mapping (verified against mongoose-os-libs/wifi):
 *   Network 1 -> wifi.sta.{ssid,pass,enable}
 *   Network 2 -> wifi.sta1.{ssid,pass,enable}
 * NEVER wifi.sta2 - that slot is the AP-provisioning fallback.
 */

import { parseArgs } from "node:util";
import { spawnSync } from "node:child_process";
import { readFileSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve } from "node:path";
import { platform } from "node:os";

const __dirname = dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = resolve(__dirname, "..");
const BUILD_DIR = join(PROJECT_ROOT, "build");
const FW_ZIP = join(BUILD_DIR, "fw.zip");
const CONF9_JSON = join(BUILD_DIR, "conf9.json");
const FW_CONF_ZIP = join(BUILD_DIR, "fw_conf.zip");
const IS_WINDOWS = platform() === "win32";
const QUOTE = String.fromCharCode(39);
const DQUOTE = String.fromCharCode(34);

const OPTIONS = {
  "wifi-ssid": { type: "string" },
  "wifi-pass": { type: "string" },
  "wifi-ssid2": { type: "string" },
  "wifi-pass2": { type: "string" },
  "mqtt-enable": { type: "boolean" },
  "mqtt-server": { type: "string" },
  "mqtt-user": { type: "string" },
  "mqtt-pass": { type: "string" },
  "device-id": { type: "string" },
  out: { type: "string" },
  "no-build": { type: "boolean", default: false },
  "keep-conf": { type: "boolean", default: false },
  "dry-run": { type: "boolean", default: false },
  help: { type: "boolean", short: "h", default: false },
};

function printHelp() {
  const lines = [
    "build_fw.mjs - build a flashable firmware bundle with baked-in config",
    "",
    "Options:",
    "  --wifi-ssid <s>     Network 1 SSID     -> wifi.sta.ssid  (+ enable)",
    "  --wifi-pass <s>     Network 1 password -> wifi.sta.pass",
    "  --wifi-ssid2 <s>    Network 2 SSID     -> wifi.sta1.ssid (+ enable)",
    "  --wifi-pass2 <s>    Network 2 password -> wifi.sta1.pass",
    "  --mqtt-enable       Enable MQTT        -> mqtt.enable = true",
    "  --mqtt-server <s>   MQTT server URL    -> mqtt.server",
    "  --mqtt-user <s>     MQTT username      -> mqtt.user",
    "  --mqtt-pass <s>     MQTT password      -> mqtt.pass",
    "  --device-id <s>     Device id          -> device.id",
    "  --out <path>        Output bundle path (default: build/fw_conf.zip)",
    "  --no-build          Reuse existing build/fw.zip (skip web + fw build)",
    "  --keep-conf         Keep build/conf9.json after bundling (has secrets!)",
    "  --dry-run           Print planned config and commands, build nothing",
    "  -h, --help          Show this help",
    "",
    "Defaults read from config.local.json or .env at repo root (gitignored);",
    "CLI flags override file defaults.",
    "",
    "The produced bundle contains plaintext credentials - do NOT commit it.",
  ];
  console.info(lines.join("\n"));
}

function loadJsonDefaults() {
  const p = join(PROJECT_ROOT, "config.local.json");
  if (!existsSync(p)) return {};
  try {
    return JSON.parse(readFileSync(p, "utf8"));
  } catch (err) {
    throw new Error("Failed to parse config.local.json: " + err.message);
  }
}

const ENV_TO_KEY = {
  WIFI_SSID: "wifiSsid",
  WIFI_PASS: "wifiPass",
  WIFI_SSID2: "wifiSsid2",
  WIFI_PASS2: "wifiPass2",
  MQTT_ENABLE: "mqttEnable",
  MQTT_SERVER: "mqttServer",
  MQTT_USER: "mqttUser",
  MQTT_PASS: "mqttPass",
  DEVICE_ID: "deviceId",
};

function stripQuotes(val) {
  const first = val.charAt(0);
  const last = val.charAt(val.length - 1);
  const dq = first === DQUOTE && last === DQUOTE;
  const sq = first === QUOTE && last === QUOTE;
  return dq || sq ? val.slice(1, -1) : val;
}

function loadEnvDefaults() {
  const p = join(PROJECT_ROOT, ".env");
  if (!existsSync(p)) return {};
  const out = {};
  for (const rawLine of readFileSync(p, "utf8").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#")) continue;
    const eq = line.indexOf("=");
    if (eq === -1) continue;
    const rawKey = line.slice(0, eq).trim();
    const val = stripQuotes(line.slice(eq + 1).trim());
    const key = ENV_TO_KEY[rawKey];
    if (!key) continue;
    out[key] = key === "mqttEnable" ? /^(1|true|yes|on)$/i.test(val) : val;
  }
  return out;
}

function resolveSettings(values) {
  const fileDefaults = Object.assign({}, loadJsonDefaults(), loadEnvDefaults());
  const pick = (flag, key) =>
    values[flag] !== undefined ? values[flag] : fileDefaults[key];
  return {
    wifiSsid: pick("wifi-ssid", "wifiSsid"),
    wifiPass: pick("wifi-pass", "wifiPass"),
    wifiSsid2: pick("wifi-ssid2", "wifiSsid2"),
    wifiPass2: pick("wifi-pass2", "wifiPass2"),
    mqttEnable:
      values["mqtt-enable"] !== undefined
        ? values["mqtt-enable"]
        : fileDefaults.mqttEnable,
    mqttServer: pick("mqtt-server", "mqttServer"),
    mqttUser: pick("mqtt-user", "mqttUser"),
    mqttPass: pick("mqtt-pass", "mqttPass"),
    deviceId: pick("device-id", "deviceId"),
  };
}

function buildConf9(s) {
  const conf = {};
  if (s.wifiSsid !== undefined) {
    conf.wifi = conf.wifi || {};
    conf.wifi.sta = {
      enable: true,
      ssid: String(s.wifiSsid),
      pass: s.wifiPass !== undefined ? String(s.wifiPass) : "",
    };
  }
  if (s.wifiSsid2 !== undefined) {
    conf.wifi = conf.wifi || {};
    conf.wifi.sta1 = {
      enable: true,
      ssid: String(s.wifiSsid2),
      pass: s.wifiPass2 !== undefined ? String(s.wifiPass2) : "",
    };
  }
  const mqtt = {};
  if (s.mqttEnable !== undefined) mqtt.enable = Boolean(s.mqttEnable);
  if (s.mqttServer !== undefined) mqtt.server = String(s.mqttServer);
  if (s.mqttUser !== undefined) mqtt.user = String(s.mqttUser);
  if (s.mqttPass !== undefined) mqtt.pass = String(s.mqttPass);
  if (Object.keys(mqtt).length > 0) conf.mqtt = mqtt;
  if (s.deviceId !== undefined) conf.device = { id: String(s.deviceId) };
  return conf;
}

function maskConf(conf) {
  const clone = JSON.parse(JSON.stringify(conf));
  if (clone.wifi && clone.wifi.sta && clone.wifi.sta.pass)
    clone.wifi.sta.pass = "***";
  if (clone.wifi && clone.wifi.sta1 && clone.wifi.sta1.pass)
    clone.wifi.sta1.pass = "***";
  if (clone.mqtt && clone.mqtt.pass) clone.mqtt.pass = "***";
  return clone;
}

function run(cmd, args, opts = {}) {
  console.info("\n$ " + cmd + " " + args.join(" "));
  const res = spawnSync(
    cmd,
    args,
    Object.assign({ stdio: "inherit", cwd: PROJECT_ROOT, shell: false }, opts)
  );
  if (res.error) throw res.error;
  if (res.status !== 0) {
    throw new Error(
      "Command failed (exit " + res.status + "): " + cmd + " " + args.join(" ")
    );
  }
}

function toWslPath(winPath) {
  if (!IS_WINDOWS) return winPath;
  const m = /^([A-Za-z]):[\/](.*)$/.exec(winPath);
  if (!m) return winPath.replace(/\\/g, "/");
  return "/mnt/" + m[1].toLowerCase() + "/" + m[2].replace(/\\/g, "/");
}

function toBuildRel(outPath) {
  const abs = resolve(outPath);
  if (abs.startsWith(BUILD_DIR)) {
    return (
      "build/" +
      abs.slice(BUILD_DIR.length).replace(/^[\\/]/, "").replace(/\\/g, "/")
    );
  }
  if (abs.startsWith(PROJECT_ROOT)) {
    return abs
      .slice(PROJECT_ROOT.length)
      .replace(/^[\\/]/, "")
      .replace(/\\/g, "/");
  }
  throw new Error(
    "--out must be inside the project directory (mounted volume): " + outPath
  );
}

function createFwBundle(outPath) {
  const wslRoot = toWslPath(PROJECT_ROOT);
  const dockerArgs = [
    "run",
    "--rm",
    "-v",
    wslRoot + ":/w",
    "-w",
    "/w",
    "mgos/mos:latest",
    "create-fw-bundle",
    "-i",
    "build/fw.zip",
    "-o",
    toBuildRel(outPath),
    "conf9:src=build/conf9.json,type=conf",
  ];
  if (IS_WINDOWS) {
    run("wsl", ["-e", "docker"].concat(dockerArgs));
  } else {
    run("docker", dockerArgs);
  }
}

function main() {
  let parsed;
  try {
    parsed = parseArgs({ options: OPTIONS, allowPositionals: false });
  } catch (err) {
    console.error("Argument error: " + err.message + "\n");
    printHelp();
    process.exit(2);
  }
  const values = parsed.values;

  if (values.help) {
    printHelp();
    return;
  }

  const settings = resolveSettings(values);
  const conf9 = buildConf9(settings);

  if (Object.keys(conf9).length === 0) {
    console.error(
      "No configuration provided. Supply at least one setting via flags, " +
        "config.local.json or .env. Use --help for details."
    );
    process.exit(2);
  }

  const outPath = values.out ? resolve(PROJECT_ROOT, values.out) : FW_CONF_ZIP;

  console.info("Planned conf9 (user layer, secrets masked):");
  console.info(JSON.stringify(maskConf(conf9), null, 2));

  if (values["dry-run"]) {
    console.info(
      "\n[dry-run] Would build web + firmware: " + !values["no-build"]
    );
    console.info("[dry-run] Would write conf9 to: " + CONF9_JSON);
    console.info("[dry-run] Would create bundle:   " + outPath);
    return;
  }

  if (!values["no-build"]) {
    run(IS_WINDOWS ? "npm.cmd" : "npm", ["run", "build:local"]);
  }

  if (!existsSync(FW_ZIP)) {
    throw new Error(
      FW_ZIP +
        " not found. Run without --no-build, or build the firmware first."
    );
  }

  writeFileSync(CONF9_JSON, JSON.stringify(conf9, null, 2) + "\n", "utf8");

  try {
    createFwBundle(outPath);
    console.info("\nBundle written: " + outPath);
    console.info(
      "Flash it with:\n  curl --digest -u admin:<password> -F file=@" +
        toBuildRel(outPath) +
        " http://<device-ip>/update"
    );
    console.info(
      "\nReminder: bundle contains plaintext credentials - do not commit or share it."
    );
  } finally {
    if (!values["keep-conf"] && existsSync(CONF9_JSON)) {
      rmSync(CONF9_JSON);
      console.info("Removed " + CONF9_JSON + " (contained secrets).");
    } else if (values["keep-conf"]) {
      console.info(
        "Kept " +
          CONF9_JSON +
          " as requested (--keep-conf). It contains secrets."
      );
    }
  }
}

main();
