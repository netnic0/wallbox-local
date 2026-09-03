#!/usr/bin/env node
/*eslint no-console: "off"*/
/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

/**
 * mos_build.mjs - build the ESP8266 firmware inside the mgos/mos Docker image.
 *
 * The Mongoose OS `mos` CLI is not required on the host: this runs
 * `mos build --local` inside the `mgos/mos:latest` container, which is the
 * approach documented in docs/BUILD-AND-FLASH.md. This makes `npm run
 * mos-build:local` work on any machine that has Docker (via WSL on Windows),
 * without installing/maintaining a native `mos` binary.
 *
 * Usage:
 *   node scripts/mos_build.mjs [--target development|production]
 *
 * Output: build/fw.zip
 *
 * Requirements:
 *   - Docker available (native on Linux/WSL, or Docker Desktop reachable from WSL).
 *   - The mgos/mos image cached locally: `docker pull mgos/mos:latest`.
 */

import { parseArgs } from "node:util";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { platform } from "node:os";

const __dirname = dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = resolve(__dirname, "..");
const IS_WINDOWS = platform() === "win32";
const IMAGE = "mgos/mos:latest";

const OPTIONS = {
  target: { type: "string", default: "development" },
  help: { type: "boolean", short: "h", default: false },
};

/* Translate a Windows path (C:\...) into the /mnt/c/... form WSL/Docker expect.
   On Linux/WSL the path is already POSIX and returned unchanged. */
function toWslPath(winPath) {
  if (!IS_WINDOWS) return winPath;
  const m = /^([A-Za-z]):[\/](.*)$/.exec(winPath);
  if (!m) return winPath.replace(/\\/g, "/");
  return "/mnt/" + m[1].toLowerCase() + "/" + m[2].replace(/\\/g, "/");
}

function run(cmd, args) {
  console.info("\n$ " + cmd + " " + args.join(" "));
  const res = spawnSync(cmd, args, {
    stdio: "inherit",
    cwd: PROJECT_ROOT,
    shell: false,
  });
  if (res.error) throw res.error;
  if (res.status !== 0) {
    throw new Error("Command failed (exit " + res.status + "): " + cmd);
  }
}

function main() {
  let values;
  try {
    ({ values } = parseArgs({ options: OPTIONS, allowPositionals: false }));
  } catch (err) {
    console.error("Argument error: " + err.message);
    process.exit(2);
  }
  if (values.help) {
    console.info(
      "mos_build.mjs - build firmware in the mgos/mos Docker image\n" +
        "  --target <development|production>  build TARGET (default: development)\n" +
        "  -h, --help                         show this help",
    );
    return;
  }

  const mount = toWslPath(PROJECT_ROOT);

  // Command run inside the container:
  //  - mark the mounted repo as a safe git dir (repo lives on a foreign-owned
  //    mount, otherwise `mos build` aborts on "dubious ownership");
  //  - build locally for esp8266 with the requested TARGET.
  const inner =
    'git config --global --add safe.directory "*" && ' +
    "mos build --local --platform esp8266 --build-var TARGET:" +
    values.target;

  const dockerArgs = [
    "run",
    "--rm",
    "--entrypoint",
    "/bin/sh",
    "-v",
    "/var/run/docker.sock:/var/run/docker.sock",
    "-v",
    mount + ":" + mount,
    "-w",
    mount,
    IMAGE,
    "-c",
    inner,
  ];

  if (IS_WINDOWS) {
    run("wsl", ["-e", "docker"].concat(dockerArgs));
  } else {
    run("docker", dockerArgs);
  }

  console.info("\nFirmware build finished. Output: build/fw.zip");
}

main();
