#!/usr/bin/env node
/*eslint no-console: "off"*/

const fs = require('fs');

const source = process.argv.length > 2 ? process.argv[2] : undefined;
const destination = process.argv.length > 3 ? process.argv[3] : undefined;

fs.copyFile(source, destination, err => {
    if (err) {
        throw err;
    }
    console.info(`${source} copied to ${destination}`);
});