# Code Review Findings - Wallbox-Local (L1 + L2-A)

> **Date**: 2026-09-02
> **Reviewers**: Sonnet (first pass) + Opus xhigh (adversarial validation)
> **Scope**: src/main.cpp, src/wb_mqtt.cpp, src/wb_power.cpp, src/wb_rpc.cpp
> **Target HW**: Shelly 1PM Gen1 (ESP8266 + HLW8012/BL0937), Mongoose OS, single-core,
> no threading, flash NAND ~100k cycles/sector, EV charging ~3.7 kW / 16 A / 230 V, 24/7.

## Purpose

Handoff to the implementing session. Read fully before touching code.
Every fix goes through Task(senior-plan-reviewer) before coding (global CLAUDE.md),
EXCEPT the trivial defensive ones marked TRIVIAL below.

---

## Findings summary (severity revised by Opus)

| # | Finding | File | Severity | Group |
|---|---------|------|----------|-------|
| 1 | RPC relay/reset endpoints unauthenticated | wb_rpc.cpp | MEDIUM | Security - do first |
| 2 | MQTT LWT missing -> ghost charging state in HA | wb_mqtt.cpp | MEDIUM | Logic |
| 3 | energy/3600 integer division loses precision | wb_power.cpp | MEDIUM | Logic |
| 4 | Flash write every 60s wears out NAND | wb_power.cpp | MEDIUM | Logic |
| 5 | sprintf on topic buffers (no bounds) | wb_mqtt.cpp | LOW (TRIVIAL) | Defensive |
| 6 | intensity (computed) and current (real) redundant | wb_mqtt/wb_power | LOW | Cleanup |
| 7 | Wallbox.GetInfo exposes mqtt_user | wb_rpc.cpp | LOW (TRIVIAL) | Security |

---

## Detailed findings

### 1. MEDIUM - RPC relay/reset endpoints are unauthenticated
File: src/wb_rpc.cpp - rpc_init() registers all handlers with no ACL.

Problem: Wallbox.SetRelay, Wallbox.Reboot, Wallbox.Reset (factory reset), Wallbox.ResetWifi
are reachable by anyone on the LAN, e.g. http://wallbox.local/rpc/Wallbox.Reset. For an EV
charger, an unauthenticated peer can start/stop the charge or wipe the config. This is the
finding the first Sonnet pass MISSED and is the highest real-world risk here.

Fix: restrict RPC via an ACL. Mongoose OS supports rpc.acl in mos.yml. Restrict the mutating
methods (SetRelay, Reboot, Reset, ResetWifi, ResetEnergy) to authenticated callers, leave
GetInfo readable. VERIFY the exact ACL mechanism against the Mongoose OS version pinned in
mos.yml before implementing - the rpc-common API differs between versions.

Constraint: PLAN says "WebUI auth OFF by default" - do NOT force auth on the read path or you
break the existing WebUI. Only mutating RPCs need protection. Ask the user about the default
(auth on/off) before locking it down - it changes UX.

### 2. MEDIUM - MQTT Last-Will-and-Testament missing -> ghost state in HA
File: src/wb_mqtt.cpp - mqtt_init() / mqtt_send_state_topic() (line ~164).

Problem: the connected field in the state topic is hardcoded true (wb_mqtt.cpp:164). If the
box loses power mid-charge (mains outage), it stops publishing but HA keeps the last RETAINED
state - including charging: true - forever. A phantom charging state on an EV charger is
misleading.

Fix: configure an MQTT LWT so the broker publishes {"connected": false} (retained) to
wallbox/<id>/state (or a dedicated wallbox/<id>/availability topic) on disconnect. In Mongoose
OS set via mqtt.will_topic / mqtt.will_message config, or mgos_mqtt_set_will at init. HA then
uses availability_topic to mark the device offline.

Decision needed: reuse the state topic for LWT (simpler, LWT payload must be valid JSON HA can
parse) OR add a dedicated availability topic (cleaner, but PLAN froze the topic list at
state/system/announce/cmd). Ask the user.

### 3. MEDIUM - power_read_energy() / 3600 loses precision (session under-counts)
File: src/wb_power.cpp - power_update().

Problem: int energy = power_read_energy() / 3600; is integer division. Any increment smaller
than one full unit is truncated to 0. On slow charging the session counter stays at 0 too
long, so meter.session_energy under-reports early in a session.

Fix: accumulate raw energy in a higher-resolution counter and convert to Wh only for display,
OR track the fractional remainder across ticks so no energy is dropped. Confirm the HLW8012
raw unit (mgos_hlw8012_readEnergy) before changing the divisor - /3600 implies Watt-seconds
(Joules); verify in the lib. Do NOT change the on-wire energy field unit (HA depends on Wh) -
fix the accumulation only.

### 4. MEDIUM - Flash write on every 60s tick wears out NAND
File: src/wb_power.cpp - power_update(), mgos_sys_config_save() at end.

Problem: mgos_sys_config_save runs each tick where energy changed (continuously during a
charge). At 1 write/60s that is ~1440 writes/day. ESP8266 flash endurance ~100k cycles/sector;
SPIFFS wear leveling mitigates but does not eliminate the risk over a 24/7 lifetime.

Fix: throttle the persistent save. Keep in-RAM meter.* updated every tick, flush to flash
every N ticks (e.g. 10 min -> 1 write/600s) and on clean events (MQTT disconnect, before
reboot). Pattern:
    static int ticks_since_save = 0;
    /* update in-RAM meter values every tick */
    if (++ticks_since_save >= 10) {   /* 10 * 60s = 10 min */
      mgos_sys_config_save(&mgos_sys_config, false, NULL);
      ticks_since_save = 0;
    }
Caveat: on an unexpected reboot you lose up to 10 min of energy accounting - acceptable for
flash longevity, but confirm with the user. Also flush on the health-check reboot path in
main.cpp process_loop() so a planned reboot does not lose data.

### 5. LOW (TRIVIAL) - sprintf on topic buffers has no bounds check
File: src/wb_mqtt.cpp - mqtt_init(), four sprintf calls (lines ~66-69).

Problem: sprintf(mqtt_announce_topic /* [50] */, "wallbox/%s/announce", device_id). In
practice device_id is MAC-derived (~12-15 chars) so it fits, but a user-set long device.id
would overflow silently (no stack canary on ESP8266).

Fix (TRIVIAL - implement without plan review): replace all four sprintf with
snprintf(buf, sizeof(buf), ...). Buffers stay the same size. Pure defensive hardening.

### 6. LOW - intensity (computed) and current (real BL0937) are redundant
File: src/wb_mqtt.cpp (MQTT_STATE template) + src/wb_power.cpp (power_update).

Problem: the state topic carries both intensity (energy/time/voltage estimate at hardcoded
230V) and current (real BL0937). The estimate is inaccurate at low load and duplicates
current. Confusing for HA sensor setup.

Fix: prefer the real current reading. Either drop intensity from the state topic, or keep it
only if the HA dashboard binds to it (CHECK before removing - PLAN froze the JSON field set
for backward-compat). If kept, document it as an estimate. Lowest priority.

### 7. LOW (TRIVIAL) - Wallbox.GetInfo exposes mqtt_user
File: src/wb_rpc.cpp - rpc_wallbox_get_info_handler, RPC_GETINFO template.

Problem: the response includes mqtt_user in clear text. Not the password, but combined with
the unauthenticated RPC path (#1) it leaks config. No functional need to expose it.

Fix (TRIVIAL): remove mqtt_user (and mqtt_server value if not needed by WebUI) from GetInfo.
Check www/app.js does not render these fields first - if it does, that section is rewritten
in L2-B anyway, so coordinate.

---

## Confirmed GOOD (do not "fix")

- json_scanf return check + free(action) on all paths in mqtt_cmd_handler.
- power_read_energy() caps at INT_MAX to avoid cast overflow.
- delta_energy < 0 handled (HLW8012 reset detection) in power_update.
- Relay forced OFF at boot (main.cpp) - correct safety default; relay state intentionally
  not persisted.
- Reboot-counter anti-boot-loop with 30s reset - robust.
- mqtt_send_topics() in mqtt_init() is harmless (guarded by is_connected()), just redundant.

---

## Recommended implementation order

Batch A - safe & fast (TRIVIAL, no logic change)
- #5 sprintf -> snprintf
- #7 remove mqtt_user from GetInfo (after checking WebUI usage)

Batch B - security (needs plan review + user decision on auth default)
- #1 RPC ACL on mutating methods

Batch C - metering/reliability logic (needs plan review, touches business logic)
- #4 flash write throttling
- #3 energy precision
- #2 MQTT LWT

Batch D - cleanup (lowest priority, may be absorbed by L2-B WebUI rewrite)
- #6 intensity/current dedup

## Before you start

1. Read PLAN.md and docs/PLAN-L1.md / docs/PLAN-L2-A.md for design intent and frozen
   constraints (app name, MQTT topic list, JSON field set - all must stay backward-compatible
   with the existing HA dashboard).
2. Every batch B/C/D change goes through Task(senior-plan-reviewer) first.
3. Build with the reference Docker command in PLAN.md; verify build/fw.zip with
   scripts/compare_fw.py before any flash.
4. Ask the user the open decisions: #1 (auth default), #2 (LWT topic choice),
   #4 (accept up to 10 min data loss on crash).