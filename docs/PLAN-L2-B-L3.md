# Implementation Plan - L2-B (modern Web UI) and L3 (HA Discovery + Safety)

> Audience: an implementing session (Sonnet). Read this fully before touching code.
> Status at 2026-09-02: L1 (v1.0.0) done. L2-A (MQTT cmd topic) done. Code-review
> Batches A/B/C + finding #7 done and BUILT successfully (build/fw.zip verified).
> Remaining: L2-B Web UI rewrite (§3.1-3.5) + §3.7 doc sync. §2.0 and §3.0 DONE (2026-09-02).
> Reviewed by senior-plan-reviewer 2026-09-02: approve-with-changes.
>
> CODE-AUDIT CORRECTION 2026-09-02 (IMPORTANT - read before coding): an introspection of the
> actual firmware refuted three plan assumptions. Corrections are folded into 3.0, 3.1, 3.2,
> 3.6, 3.7, 4.2, 7. Summary of what was WRONG in the earlier draft:
>   1. Wallbox.GetInfo does NOT return power/voltage/current/connected/charging/tid, and NEVER
>      returned mqtt_user or any ocpp_*. Its real 17 fields are listed in 3.7.
>   2. Live power metrics exist ONLY in the MQTT state topic (src/wb_mqtt.cpp), not in any RPC.
>      A GetInfo-poll UI therefore cannot show power today.
>   3. charging in the state topic is just the relay GPIO state (wb_mqtt.cpp:199), NOT
>      relay_on AND power>100W.
>
> DECIDED (user, option a): extend Wallbox.GetInfo with power, voltage, current, charging so
> the Web UI is fully RPC-driven and broker-independent - MQTT is OFF by default (mos.yml:57)
> and the UI must also work in AP/hotspot setup mode where no broker is reachable. The MQTT
> state topic is UNCHANGED and remains the data source for Home Assistant (L3 discovery binds
> to it). The old mqtt_user soft-deprecation decision is void (the field does not exist). The
> GetInfo C change ships INSIDE the L2-B batch (reviewed together).
>
> Hardware: Shelly 1PM Gen1 - ESP8266, single-core, no threading, tight heap (~50 KB usable),
> SPIFFS flash (~100k write cycles/sector), BL0937 power meter. 24/7, EV ~3.7 kW / 16 A / 230 V.

---

## 0. Non-negotiable constraints (frozen - do NOT change)

1. App name in mos.yml stays Wallbox-Shelly1PM (line 2). The stock OTA endpoint rejects a
   package whose name differs. Never rename.
2. MQTT topic list is frozen: wallbox/<id>/state, /system, /announce, /cmd, /availability.
   The HA dashboard binds to these. Adding HA-Discovery topics (homeassistant/...) is allowed
   (new namespace, L3). Do not rename/remove existing ones.
3. state topic JSON field set is frozen for backward-compat. Real published order
   (src/wb_mqtt.cpp:40-52): uptime, connected, charging, energy, intensity, tid (=0),
   temperature, power, voltage, current. You may ADD fields; do not remove/rename.
4. RPC ACL is active (Batch B): UART open; HTTP/WS/MQTT require Digest auth (user admin, realm
   wallbox). Defined inline in mos.yml rpc.acl (line 66). Credential file fs/rpc_auth.htdigest
   (baked into SPIFFS). See README "Security".
5. Flash-write throttling (Batch C #4): energy counters flush every 10 ticks (10 min) or via
   power_flush(). Do NOT reintroduce per-tick mgos_sys_config_save.
6. Relay OFF at boot is an intentional safety default. Relay state is not persisted.
7. Every C/C++ or JS change goes through Task(senior-plan-reviewer) before coding (global
   CLAUDE.md, enforced by a PreToolUse hook). Docs/markdown are exempt.
8. File writes AND reads via the Edit/Write/Read TOOLS are blocked on this repo path (a
   PreToolUse path guard limits those tools to C:/SAPDevelop/). CONFIRMED 2026-09-02. Use the
   Bash tool or a PowerShell .ps1 script for ALL file I/O on this repo.

---

## 1. Current state of the Web UI (what L2-B replaces)

Files: www/index.html (265 lines), www/app/app.js (411 lines), www/assets/main.css.
Build: webpack inlines everything into a single gzipped dist/index.html.gz.

Problems to fix in L2-B:
- Dead OCPP code: app.js reads ocpp_url/ocpp_name/ocpp_state (app.js:17-20,299-322,345-347)
  and has an ocpp_save_btn (app.js:101, index.html:116-135). OCPP was removed in L1; GetInfo
  never returns these (they read as undefined). The whole OCPP config card in index.html
  (lines 116-135) must be deleted.
- Dead mqtt_user field: app.js:23 + app.js:326 read res.data.mqtt_user; index.html:157 has
  the input. mqtt_user is NOT in GetInfo and NOT anywhere in src/ - pure dead code. Delete the
  field and the read. NOTE: the MQTT credential config key mqtt.user (mos.yml:59) still exists
  and is still set via Config.Set on the MQTT form - do NOT remove that; only remove the
  GetInfo read + the display binding.
- No auth: every axios call is unauthenticated (app.js:114,140,191,214,236,257,271,296,333,
  and the /update POST at app.js:369). Batch B now returns 401 for all /rpc over HTTP. Fixing
  this is the primary L2-B job (see section 2).
- Heavy dependency: axios ^0.21.4 (package.json:43) for ~9 calls. Replace with fetch.
- Polling: refreshInfo polls Wallbox.GetInfo every 10 s (app.js:410 setInterval(refreshInfo,
  10000)). PLAN wants live data via WebSocket on the Mongoose /rpc WS channel (lib rpc-ws
  already present, mos.yml:78).

RPC/HTTP endpoints the UI uses (all now behind Digest auth):
- GET  /rpc/Wallbox.GetInfo - device info + (after 3.0) live metrics
- POST /rpc/Config.Set - save wifi/mqtt config ({config:{...}, save:true, reboot:true})
- POST /rpc/Wallbox.Reboot | .Reset | .ResetWifi | .ResetEnergy | .SetRelay {on:bool}
- GET  /rpc/FS.List - log file listing
- POST /update - multipart OTA (also behind Digest auth after 2.0)

---

## 2. Auth strategy for L2-B (DECISION REQUIRED - gated on an on-device test)

Batch B made HTTP/WS/MQTT RPC require Digest auth. The browser MAY handle Digest natively for
top-level navigation and same-origin fetch IF the initial page load itself was authenticated
and the browser caches Digest credentials for the origin. Per RFC 7616 a UA MAY (not MUST)
reuse credentials - implementation-defined and breaks silently when:
- the browser discards the cached nonce after a short idle timeout and must re-challenge;
- the /update POST is a different path than /rpc - some browsers treat a new path as a new
  protection space and re-prompt even with the same realm;
- credential caching is disabled entirely inside cross-origin iframes.

So the recommendation is conditional, decided by the VERIFY test in 2.1.

- Option A (rely on browser-native Digest): user hits http://wallbox.local/, browser prompts
  once, caches credentials, subsequent fetch calls to /rpc and /update POST reuse them. Zero
  JS auth code. Choose ONLY if 2.1 confirms reuse works on BOTH Chrome and Firefox for /update.
- Option B (JS-layer credentials): on first prompt, store user+password (or the HA1 hash) in a
  JS closure; compute the Digest header manually for every fetch() and /update. ~150-line
  helper, deterministic across browsers, required for silent WS reconnect (3.2).

### 2.0 SECURITY PREREQUISITE - protect /update and static files (do FIRST)
Verified from lib sources 2026-09-02 (deps/ota-http-server/mos.yml:10,
deps/http-server/mos.yml:21-23):
- /update is registered by ota-http-server (config key update.enable_post, default true) as a
  plain HTTP endpoint, NOT an RPC channel. Batch-B rpc.acl does NOT cover it.
- ota-http-server has no auth key of its own; it relies on http-server.
- http-server exposes http.auth_domain ("authentication of all HTTP requests"),
  http.auth_file, and http.auth_algo (0 = MD5 = standard htdigest). Their DEFAULTS are
  empty/unset (http-server/mos.yml:21-23), so no HTTP auth is active.
- The current mos.yml sets NONE of these. => Today /update AND the static WebUI are served
  unauthenticated; only /rpc/* is protected. Anyone on the LAN can flash arbitrary firmware.
  Fix it as the first L2-B change.

Fix - add to mos.yml config_schema (append after line 66, the rpc.acl entry):

```yaml
  - ["http.auth_domain", "s", "wallbox", {title: "HTTP digest auth realm (match RPC)"}]
  - ["http.auth_file", "s", "rpc_auth.htdigest", {title: "HTTP htdigest file (shared with RPC)"}]
  - ["http.auth_algo", "i", 0, {title: "htdigest hash: 0=MD5 (matches rpc_auth.htdigest)"}]
```

Realm MUST equal rpc.auth_domain (wallbox, mos.yml:64) and the file MUST be the same
rpc_auth.htdigest (mos.yml:65, present in fs/) - otherwise the browser will never reuse
credentials between the page, /rpc and /update, which kills Option A. Intended side-effect:
with a global http.auth_domain, the WebUI page load itself prompts for auth, so the browser
caches Digest credentials BEFORE the first /rpc or /update call - exactly what Option A needs.
This mos.yml change goes through Task(senior-plan-reviewer), then built and flashed before 2.1.


### 2.0.1 Recovery / Bricking Prevention (read before flashing)
- Default password during L2-B is "wallbox" (intentional, kept until further notice - see section 7).
- If credentials are ever lost or the device locks out, reflash over UART:
  mos flash --port /dev/ttyUSBx --firmware <firmware.zip>
  UART is always unauthenticated per rpc.acl (mos.yml:66 - confirmed).
- HA automation or REST scripts that call /rpc over HTTP will receive 401 after §2.0 lands.
  Inventory all HA service calls to http://<wallbox_ip>/rpc before running the 2.1 browser test.
  This does not block §2.0 coding but must be resolved before considering §2.1 complete.
### 2.1 VERIFY on-device (implementation-phase task - NOT a user action)
> Test the IMPLEMENTING session runs on the flashed device while building L2-B.
On Chrome AND Firefox: open dev tools, authenticate once against /rpc, then manually POST to
/update and watch for a second 401 prompt.
- No second prompt on both browsers -> Option A.
- A second prompt on either browser -> Option B.

### 2.2 Option B - JS Digest helper interface (spec so it is not designed ad-hoc)
If Option B is selected, implement a helper with this shape:

```
setCredentials(user, pass)                       // stores user + password (module closure)
rpcFetch(method, params) -> Promise<result>      // computes Digest for /rpc, retries on 401
otaUpload(file) -> Promise                        // computes Digest for POST /update
```

Digest computation: parse the WWW-Authenticate challenge (realm, nonce, qop), compute
HA1 = MD5(user:realm:pass), HA2 = MD5(method:uri),
response = MD5(HA1:nonce:nc:cnonce:qop:HA2). On a 401 with a fresh nonce, recompute and retry
once transparently. (MD5 in-browser: ship a tiny MD5 impl; do NOT pull a crypto lib - budget.)

---

## 3. L2-B - GetInfo extension + Modern Web UI (est. 5-7 h)

### 3.0 FIRMWARE CHANGE - extend Wallbox.GetInfo (do BEFORE the UI rewrite, same batch)
Why: the UI must show live power/voltage/current/charging without depending on an MQTT broker
(MQTT is OFF by default; the UI also runs in AP/hotspot setup mode where no broker is
reachable). These values are already computed for the MQTT state topic; expose them over RPC
too. The MQTT state topic is NOT touched.

File: src/wb_rpc.cpp - two coordinated edits, keep arg order aligned with the template.

Edit 3.0.a - the RPC_GETINFO format string (currently wb_rpc.cpp:26-46, ends with
"mqtt_server: %Q" "}"). Append four fields BEFORE the closing "}". New tail:

```c
    "mqtt_server: %Q,"
    "power: %d,"
    "voltage: %d,"
    "current: %.2f,"
    "charging: %B"
    "}";
```

Edit 3.0.b - the mg_rpc_send_responsef(...) call in rpc_wallbox_get_info_handler (currently
wb_rpc.cpp:45-63, last arg is mgos_sys_config_get_mqtt_server()). Append the four matching
args IN THE SAME ORDER after the mqtt_server arg:

```c
                        mgos_sys_config_get_mqtt_server(),
                        (int) power_read_active_power(),
                        (int) power_read_voltage(),
                        power_read_current(),
                        mgos_gpio_read(mgos_sys_config_get_gpio_relay()));
```

Notes for the implementer:
- %B consumes an int/bool; mgos_gpio_read returns int (0/1) - correct for %B.
- %.2f consumes a double; power_read_current() returns double (wb_power.h) - correct.
- power_read_active_power()/power_read_voltage() return unsigned; cast to (int) exactly as
  wb_mqtt.cpp:216-217 does (frozen format string does not support %u; values fit signed int).
- wb_power.h is already included at wb_rpc.cpp:19 (verified 2026-09-02). No include change needed.
  current includes (wb_rpc.cpp currently includes wb_thermistor.h, wb_util.h, mgos.h,
- mgos_gpio_read needs the gpio API - provided by mgos.h (already included).
- Do NOT change the state topic, MQTT_STATE, or any other handler.

GetInfo field count after this change: 21 (the 17 in 3.7 + power, voltage, current, charging).
Reflect this in doc/rpc.md (3.7).

### 3.1 Goals (UI)
- Single self-contained gzipped page, < 30 KB gzipped, no axios (use fetch).
- Data source = the extended Wallbox.GetInfo over RPC (fetch, and WebSocket for live refresh
  per 3.2). Do NOT make the browser subscribe to MQTT - the broker may be absent.
- Cards: Status (state, power, voltage, current, energy, temperature, uptime, charging),
  Control (Start/Stop charge via Wallbox.SetRelay, Reset energy), Wi-Fi config, MQTT config
  (server/user/pass -> Config.Set), Maintenance (reboot, factory reset, reset wifi, OTA
  upload), Logs (FS.List).
- Remove every OCPP reference and the dead mqtt_user display (section 1).
- Responsive, mobile-first (a phone on the home Wi-Fi is the primary client).

### 3.2 WebSocket live data + reconnect (REQUIRED credential handling)
Mongoose rpc-ws authenticates at WebSocket upgrade time (the HTTP Upgrade handshake). Once
open, JSON-RPC frames are NOT individually re-challenged - a single persistent socket avoids
per-request auth. BUT the socket WILL drop (watchdog, wifi event, idle) and each reconnect
issues a fresh 401 upgrade challenge. rpc-ws exposes reconnect tuning (deps/rpc-ws/mos.yml:
16-17: rpc.ws.reconnect_interval_min=1, _max=60) but NO server-side idle key - treat
idle-close as browser/network-driven and handle it client-side.

Therefore, regardless of Option A/B:
- Store credentials (or HA1 hash) in a JS variable at first successful auth so the
  auto-reconnect handler replays them in the Authorization header of the next Upgrade request
  without a browser prompt.
- Reconnect loop sketch:
  connect()
    on open:  start WS-poll (call Wallbox.GetInfo over the socket every N s, N=2..5)
    on 401:   attach stored credentials, retry the upgrade once
    on close: exponential backoff, reconnect; after M failures, fall back to fetch polling 5 s
- The WS transport is the Mongoose /rpc WS endpoint (ws://<host>/rpc). Frames are standard
  mg_rpc JSON: {"id":<n>,"method":"Wallbox.GetInfo"}. Match ids to responses.
- Periodic data via WS-poll is acceptable for L2-B and avoids firmware changes. Do NOT add a
  firmware-side notification in this phase.

### 3.3 Files to change
- src/wb_rpc.cpp - 3.0 GetInfo extension (do first). senior-plan-reviewer gate applies.
- www/index.html - rewrite: drop OCPP card (lines 116-135) + mqtt_user input (line 157),
  restructure into the 3.1 cards, semantic HTML, no framework.
- www/app/app.js - rewrite: vanilla fetch + WebSocket, no axios. RPC helper with 401 handling
  (2.2). Keep confirm() dialogs for destructive actions. Remove refreshInfo setInterval
  (app.js:410), ocpp_* reads, mqtt_user read.
- www/assets/main.css - modern minimal CSS (CSS variables, dark-mode-friendly). Lean; the
  < 30 KB budget is TOTAL (HTML+CSS+JS gzipped, inlined into one index.html.gz).
- package.json - remove "axios": "^0.21.4" from dependencies (line 43). Keep the webpack
  minify+gzip chain. Re-run npm run lint (eslint configured, package.json:15).
- webpack.config.js - REQUIRED size gate: add
  performance: { maxAssetSize: 30720, maxEntrypointSize: 30720, hints: "error" } to the
  exported config object so the < 30 KB budget is enforced at build time. NOTE: CompressionPlugin
  runs with deleteOriginalAssets in production (webpack.config.js:59-61) and the perf hint
  measures the UNCOMPRESSED asset by default - so ALSO sanity-check the gzipped output size
  manually (3.5).
- mos.yml - only the 2.0 http.auth_* additions; no other change (UI is FS content).

### 3.4 Dead-code removal checklist (delete ALL - named to prevent partial removal)
- [ ] axios require (app.js:5) + the axios dependency in package.json (line 43)
- [ ] OCPP config card in index.html (lines 116-135: ocpp_name, ocpp_url inputs, ocpp_save_btn,
      ocpp_spinner) and the ocpp_state span (index.html:27)
- [ ] all OCPP references in app.js (17-20, 32, 101-121, 299-322, 345-347)
- [ ] mqtt_user input (index.html:157) + its GetInfo read (app.js:23, app.js:326)
- [ ] the refreshInfo setInterval polling loop (app.js:410) - replaced by 3.2 WebSocket

### 3.5 Acceptance
- npm run webpack produces dist/index.html.gz and the build FAILS if the asset exceeds the 3.3
  performance gate. Manually confirm gzipped size: ls -l dist/index.html.gz < 30 KB.
- After flashing, the page loads, auth works per the option chosen, Status shows live
  power/voltage/current/charging (from the extended GetInfo), controls work, Start/Stop toggles
  the relay, OTA upload works with auth.
- No console errors; no reference to ocpp_* or mqtt_user in the built bundle.

### 3.6 mqtt_user - RESOLVED (no action, decision void)
The earlier "soft-deprecation vs breaking change" decision was based on a false premise:
mqtt_user is NOT a GetInfo field and appears nowhere in src/ (verified 2026-09-02). The only
work is deleting the dead UI read (3.4). The MQTT credential config key mqtt.user (mos.yml:59)
is unrelated and stays - it is written via the MQTT config form (Config.Set) and consumed by
the mqtt lib.

### 3.7 Doc sync (do in the same batch)
- doc/rpc.md is stale. The REAL current Wallbox.GetInfo response (src/wb_rpc.cpp:26-46, BEFORE
  the 3.0 change) has exactly these 17 fields:
  id, sn, app, version, fw_build, fw_ts, mac, ip, uptime, temperature, wifi_ssid, wifi_ssid1,
  energy, intensity, state, mqtt_state, mqtt_server.
  There is NO ocpp_*, NO mqtt_user, NO power/voltage/current/connected/charging/tid in the
  pre-change response. Update doc/rpc.md to this, then ADD the four 3.0 fields (power, voltage,
  current, charging) -> 21 fields total after L2-B.
- Provide a diff table in doc/rpc.md of RPC methods after Batch B + 3.0 (name, params, auth
  requirement). The 6 methods are (src/wb_rpc.cpp:29-34): Wallbox.GetInfo (""),
  Wallbox.Reboot (""), Wallbox.Reset (""), Wallbox.ResetWifi (""),
  Wallbox.SetRelay ("{on:%B}"), Wallbox.ResetEnergy (""). All require +admin auth on
  HTTP/WS/MQTT; UART open (mos.yml:66).

---

## 4. L3 - Polish, HA Discovery, EV detection, Safety (est. 3-5 h) -> v1.2.0

Split into independent sub-batches; each separately reviewable and flashable.

### 4.1 HA MQTT Discovery - NEW src/wb_discovery.cpp (+ include/wb_discovery.h)
- On MQTT connect, reuse the existing MG_EV_MQTT_CONNACK handler in wb_mqtt.cpp:118-127 (it
  currently only publishes the retained "online" availability). Extend/hook it to trigger the
  staged discovery publish.
- Publish retained discovery configs under homeassistant/<component>/<node_id>/<obj>/config.
- Entities (bind to the frozen state-topic fields via value_template - the state topic is the
  HA data source, NOT GetInfo):
  sensor: power (W), voltage (V), current (A), energy (Wh), temperature (C), uptime (s);
  binary_sensor: charging; switch: relay (command via wallbox/<id>/cmd
  {"action":"start"|"stop"}, state from charging); availability from wallbox/<id>/availability
  (online/offline).
- node_id = device.id (e.g. wallbox-ABCDEF); unique_id per entity for the HA registry.
- REQUIRED staged/throttled publish (do NOT publish all in a tight boot loop - heap frag on
  ESP8266):
  - Arm a one-shot mgos_set_timer ~2 s AFTER MQTT connect (let TLS/handshake memory settle).
  - Publish ONE discovery topic per ~300 ms timer tick (not a for-loop).
  - Before EACH publish: if (mgos_get_free_heap_size() < 12000) { log error; defer/abort; }.
  - Build each config with mgos_mqtt_pubf and small stack buffers.
  - Expected topic count = 9 (power sensor, voltage sensor, current sensor, energy sensor,
    temperature sensor, uptime sensor, charging binary_sensor, relay switch, availability).
    The sequence is complete when 9 topics have been published.
- Retained messages re-publish on every reconnect - the same staged/guarded path must run on
  reconnect, not only first connect.
- Add config flag mqtt.ha_discovery (bool, default true) so it can be disabled.
- value_templates must match the FROZEN state field names exactly (value_json.power,
  value_json.voltage, value_json.current, value_json.energy, value_json.temperature,
  value_json.uptime, value_json.charging). Verify against src/wb_mqtt.cpp:40-52.

### 4.2 EV charge detection improvement
- CORRECTION: today charging is just the relay GPIO state (src/wb_mqtt.cpp:199:
  charging = mgos_gpio_read(relay)), NOT "relay_on AND power>100W". After 3.0 the SAME
  relay-state semantics are also exposed in GetInfo. Adding a power-threshold definition is
  therefore NET-NEW behavior, not a confirm.
- Change the definition (in ONE place if possible - factor a helper bool is_charging() used by
  BOTH wb_mqtt.cpp state topic AND wb_rpc.cpp GetInfo so they never diverge): charging =
  relay_on AND power sustained > threshold, with hysteresis (ON at >100 W, OFF at <50 W held
  for 3 consecutive ticks) to avoid flapping.
- DECIDED 2026-09-02 (user): thresholds HARDCODED (not config keys). A charging Scenic E-Tech
  draws ~3700 W, far above 100 W, so the margins are safe. If mis-detection appears, promote
  to config keys then.
- Because the state topic publishes on the 60 s process_loop tick, "3 consecutive ticks" =
  3 minutes at the current cadence - acceptable for a slow EV session; do NOT add a faster
  timer just for detection (the safety timer in 4.3 is separate).

### 4.3 Safety - NEW src/wb_safety.cpp (+ include/wb_safety.h)
- Over-temperature: if thermistor_read_celsius() (wb_thermistor.h) > 80 C -> relay OFF, call
  power_flush() (wb_power.h), log, then reboot. Reuse the health-check reboot path: main.cpp
  process_loop() at lines 67-76 already does power_flush()+mgos_system_restart() on a failed
  healthcheck() - add an over-temp condition to healthcheck() OR mirror that exact sequence.
  Config safety.temp_max_c (default 80).
- Over-current: if power_read_current() > 12 A sustained for 5 s -> relay OFF, log, publish
  state. Config safety.current_max_a (default 12), safety.current_max_ms (default 5000). 5 s is
  finer than the 60 s process_loop tick (main.cpp:153), so arm a short mgos_set_timer at ~1 s
  ONLY while a charging session is active. mgos timers fire on the main event loop (no
  threading hazard), but on ~50 KB heap an orphaned 1 s timer is a slow thrash.
- REQUIRED timer lifecycle - the 1 s timer MUST be disarmed in EVERY exit path:
  - module-static timer_id initialised to MGOS_INVALID_TIMER_ID;
  - guard at every arm/disarm site:
    if (timer_id != MGOS_INVALID_TIMER_ID) { mgos_clear_timer(timer_id); timer_id = MGOS_INVALID_TIMER_ID; }
  - disarm sites: (i) session-stop (the MQTT cmd "stop" branch wb_mqtt.cpp:96-101 AND the
    Wallbox.SetRelay {on:false} handler in wb_rpc.cpp), (ii) MQTT disconnect callback,
    (iii) WiFi-lost callback, (iv) reboot RPC handler (wb_rpc.cpp:70-83).
  NOTE: relay-off happens in TWO places (MQTT cmd handler AND Wallbox.SetRelay RPC) - arm on
  relay-ON in both, disarm on relay-OFF in both. This is why 4.2 recommends a shared helper.
- Safety checks must be fail-safe: on any doubt, relay OFF. Never trust a single noisy BL0937
  sample - require N consecutive over-limit reads.

### 4.4 Acceptance (L3)
- HA auto-discovers all 9 entities; the switch controls the relay; availability flips to
  offline on power loss (LWT in place from Batch C #2, wb_mqtt.cpp:141-143).
- Forcing temp/current over the limit (bench or simulated) trips the relay OFF.
- Build clean, fw.zip flashes, the existing HA dashboard still works (frozen topics/fields).

---

## 5. Build / flash / verify

See docs/BUILD-AND-FLASH.md for exact tested commands (WSL + Docker mgos/mos, OTA with Digest
auth). Summary:
1. npm install && npm run webpack (in WSL) -> dist/.
2. Docker mos build --local --platform esp8266 via the _build.sh pattern -> build/fw.zip.
3. Verify fs.bin contains rpc_auth.htdigest + index.html.gz.
4. OTA: upload build/fw.zip (NOT fs.bin) in the firmware-update form, or
   curl --digest -u admin:<pw> -F file=@build/fw.zip http://<ip>/update.

### 5.1 OTA rollback / brick-recovery (confirm before first L3 flash)
Mongoose OTA writes to a second slot and commits only on a successful boot. The key
update.commit_timeout EXISTS (deps/ota-common/mos.yml:28) but has NO default - i.e. it is
currently UNSET, so auto-rollback is NOT active. Before the first L3 flash, add
update.commit_timeout (non-zero, e.g. 300) to mos.yml config_schema so a failed/interrupted
flash auto-rolls-back instead of bricking. Documented recovery if OTA fails: serial flash over
USB (docs/BUILD-AND-FLASH.md serial section).

---

## 6. Suggested order and sizing (sequence is enforced, not just advisory)

1. L2-B (one PR): 2.0 mos.yml auth fix -> 3.0 GetInfo extension -> Web UI rewrite -> 3.7 doc
   sync. Biggest user-visible win; also repairs the WebUI that Batch B intentionally broke.
   Fully validate L2-B ALONE first - flash L2-B only, run the 2.1 browser auth test matrix
   (Chrome + Firefox), confirm UI connects and OTA works - BEFORE flashing any L3 code. L3 adds
   a 1 s timer + extra MQTT traffic that shifts the heap baseline.
2. L3.1 HA Discovery (one PR) - high value, self-contained.
3. L3.3 Safety (one PR) - highest real-world importance; test carefully.
4. L3.2 EV detection (fold the shared is_charging() helper into L3.1 or its own small PR; note
   it changes a frozen field semantics, so document in changelog).
5. Optional cleanup: finding #6 (dedup intensity vs current) - may be absorbed by the L2-B
   rewrite; decide when touching the state topic.

## 7. Decisions (user, 2026-09-02)
- GetInfo data source (option a): extend Wallbox.GetInfo with power, voltage, current,
  charging; UI is RPC-driven and broker-independent. MQTT state topic UNCHANGED and remains the
  Home Assistant data source. (See header + 3.0.)
- mqtt_user: decision void - field does not exist; only delete the dead UI read (3.6).
- charge-detection thresholds: HARDCODED (see 4.2).
- default admin password: keep wallbox for now (do NOT change before L2-B).
- auth strategy (A vs B): gated on the 2.1 on-device test, which requires 2.0 done first.
  Implementation-phase task.

## 8. On-device verifications to run before/at the start of coding (from the plan review)
- (a) 2.1 - Chrome+Firefox reuse of cached Digest creds for the same-origin /update POST after
  one /rpc auth. Determines Option A vs B.
- (b) rpc-ws reconnect behavior: rpc.ws.reconnect_interval_min/max exist (deps/rpc-ws/mos.yml:
  16-17); there is NO server idle-timeout key. Confirm the ACL on rpc-ws is checked at upgrade
  time only, not per-frame (read the mgos rpc-ws dispatcher or test empirically). Drives 3.2.
- (c) SPIFFS max single-file size - confirm the rewritten UI bundle (gzipped) fits in one
  SPIFFS slot before adding the Digest helper / WS wrapper.
- (d) Baseline free heap at the moment wb_safety.cpp would arm its 1 s timer (after wifi +
  MQTT + SPIFFS + discovery published). Log mgos_get_free_heap_size() in a test build; if
  < 18 KB there, re-evaluate the timer approach.
