#!/usr/bin/env node
/*eslint no-console: "off"*/

const config = require("../doc/wallbox-configuration.json");
const configurationKeys = config.configurationKey;

console.info('# Configuration keys\n');
console.info(`| Key | Value | Read-only |`);
console.info(`|---|---|---|`);

for (let [, key] of Object.entries(configurationKeys)) {
    console.info(`| ${key.key} | ${key.value} | ${key.readonly} |`);
}
