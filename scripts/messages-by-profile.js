#!/usr/bin/env node
/*eslint no-console: "off"*/

const doc = require("../doc/feature-profiles.json");
const featureProfiles = doc.FeatureProfiles;

console.info(`## Messages by feature profile\n`);

featureProfiles.forEach(profile => {
    console.info(`### ${profile.Name}\n`);
    const messages = profile.Messages;
    messages.forEach(message => {
        console.info(`- ${message.Name}`);
    })
    console.info(``);
});
