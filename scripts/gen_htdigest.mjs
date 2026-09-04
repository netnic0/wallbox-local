#!/usr/bin/env node
/*eslint no-console: "off"*/
/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

/**
 * gen_htdigest.mjs - generate the fs/rpc_auth.htdigest credential file.
 *
 * The file protects the Web UI, /rpc and /update (OTA) via HTTP Digest auth.
 * It is intentionally gitignored (contains a credential hash) and generated
 * locally at build time so no default password ships in the repository.
 *
 * htdigest line format:  user:realm:MD5(user:realm:password)
 * The realm MUST match http.auth_domain / rpc.auth_domain in mos.yml ("wallbox").
 *
 * Usage:
 *   node scripts/gen_htdigest.mjs --user admin --pass <password> [--realm wallbox] [--out fs/rpc_auth.htdigest]
 *
 * Password source (first found wins):
 *   --pass <p>          explicit flag
 *   WALLBOX_ADMIN_PASS  environment variable
 *
 * If the target file already exists it is NOT overwritten unless --force is
 * passed, so a build never silently clobbers an operator-set credential.
 */

import { parseArgs } from "node:util";
import { createHash } from "node:crypto";
import { writeFileSync, existsSync, mkdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve } from "node:path";

const __dirname = dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = resolve(__dirname, "..");
const DEFAULT_OUT = join(PROJECT_ROOT, "fs", "rpc_auth.htdigest");

const OPTIONS = {
  user: { type: "string", default: "admin" },
  pass: { type: "string" },
  realm: { type: "string", default: "wallbox" },
  out: { type: "string" },
  force: { type: "boolean", default: false },
  help: { type: "boolean", short: "h", default: false },
};

function printHelp() {
  console.info(
    [
      "gen_htdigest.mjs - generate fs/rpc_auth.htdigest (HTTP/RPC digest auth)",
      "",
      "Options:",
      "  --user <s>   Username (default: admin)",
      "  --pass <s>   Password (or set WALLBOX_ADMIN_PASS env var)",
      "  --realm <s>  Digest realm; must match mos.yml auth_domain (default: wallbox)",
      "  --out <s>    Output path (default: fs/rpc_auth.htdigest)",
      "  --force      Overwrite an existing credential file",
      "  -h, --help   Show this help",
    ].join("\n"),
  );
}

function main() {
  let values;
  try {
    ({ values } = parseArgs({ options: OPTIONS, allowPositionals: false }));
  } catch (err) {
    console.error("Argument error: " + err.message + "\n");
    printHelp();
    process.exit(2);
  }

  if (values.help) {
    printHelp();
    return;
  }

  const user = values.user;
  const realm = values.realm;
  const pass = values.pass || process.env.WALLBOX_ADMIN_PASS;

  if (!pass) {
    console.error(
      "No password provided. Use --pass <password> or set WALLBOX_ADMIN_PASS.",
    );
    process.exit(2);
  }

  const outPath = values.out ? resolve(PROJECT_ROOT, values.out) : DEFAULT_OUT;

  if (existsSync(outPath) && !values.force) {
    console.info(
      outPath + " already exists; leaving it untouched (use --force to overwrite).",
    );
    return;
  }

  // htdigest uses MD5(user:realm:password). MD5 is mandated by the digest-auth
  // scheme and by http.auth_algo=0 in mos.yml; it is not used as a general hash.
  const ha1 = createHash("md5").update(`${user}:${realm}:${pass}`).digest("hex");
  const line = `${user}:${realm}:${ha1}\n`;

  mkdirSync(dirname(outPath), { recursive: true });
  writeFileSync(outPath, line, { encoding: "utf8", mode: 0o600 });

  console.info(`Wrote ${outPath} (user=${user}, realm=${realm}).`);
  console.info("This file is gitignored - do not commit it.");
}

main();
