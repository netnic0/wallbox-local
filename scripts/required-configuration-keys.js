#!/usr/bin/env node
/*eslint no-console: "off"*/

const doc = require("../doc/feature-profiles.json");
const featureProfiles = doc.FeatureProfiles;

console.info(`## Required configuration keys by feature profile\n`);

featureProfiles.forEach(profile => {
    console.info(`### ${profile.Name}\n`);
    const keys = profile.ConfigurationKeys;
    keys.forEach(key => {
        if (key.Required !== "optional") {
            console.info(`- ${key.Key} (type: ${key.Type}) (units: ${key.Unit ? key.Unit : '-'})`);
        }
    })
    console.info(``);
});
