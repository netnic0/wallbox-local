# Livraison 2-A — MQTT command topic + Home Assistant integration

> **Status: V2 AMENDED — APPROVED 2026-06-04**
> Reviewed by `senior-plan-reviewer` (confidence 89%, approved with amendments).
> Validated by user 2026-06-04 with the 4 mandatory amendments below.

## V2 Amendments (binding for implementation)

The 4 review amendments below **override** the corresponding sections of the original V1 plan further down. When in doubt, the V2 amendments are authoritative.

### AM-1 [CRITICAL] No CONNACK hook — re-sub is automatic

The plan's original Step 4 ("Hook subscription on `MG_EV_MQTT_CONNACK`") is REMOVED. Verified by the reviewer in the actual lib source (`deps/mqtt/src/mgos_mqtt_conn.c:599-610` and `:250-253`): `mgos_mqtt_sub()` stores the subscription in a linked list and the library iterates that list automatically on every `MG_EV_MQTT_CONNACK code=0`, calling `do_sub()` for each entry. Calling `mgos_mqtt_sub()` once at init is sufficient. Hooking CONNACK manually would cause a double-subscribe → handler called twice per message.

**Implementation rule**: call `mgos_mqtt_sub(mqtt_cmd_topic, mqtt_cmd_handler, NULL)` exactly once, in `mqtt_init()`, after the `sprintf` for the topic.

### AM-2 [UX] Immediate state publish after each cmd action

Without this, HA sees the relay's old state for up to 60s (until next periodic publish), and the switch entity in HA appears stuck. Call `mqtt_send_state_topic()` immediately after each successful action in the cmd handler.

### AM-3 [Factoring] `power_do_reset_energy()` helper

Extract the 5-line reset-energy sequence currently in `wb_rpc.cpp:179-184` (= `power_reset_energy()` + 4× `mgos_sys_config_set_meter_*` + save) into a new `power_do_reset_energy()` function declared in `include/wb_power.h` and defined in `src/wb_power.cpp`. Both `Wallbox.ResetEnergy` RPC handler and the new MQTT cmd handler call this. Avoids code duplication.

### AM-4 [Doc] HA 2024+ modern syntax only

The user runs Home Assistant 2026+. The deprecated `switch: → platform: mqtt` syntax is removed. Use the modern `mqtt: → switch:` block format only.

### User decisions ratified 2026-06-04

- Topic name : `wallbox/<id>/cmd`
- Q1 (ack topic) → (a) NO ack topic. State_topic feedback is sufficient (especially with AM-2 immediate publish).
- Q2 (re-sub on reconnect) → no manual handling needed (AM-1 explains why).
- Q3 (QoS) → moot — `mgos_mqtt_sub()` does not expose QoS, internally uses QoS 1 (which is what we want).
- Q4 (in-payload auth) → (a) NO in-payload auth. Broker-level credentials are the security boundary.
- Q-HA (HA version) → HA 2026+. Modern syntax only.

### V2 Updated Implementation Plan (overrides V1 §8)

| Step | File | Action | LoC delta |
|---|---|---|---|
| 1 | `include/wb_power.h` | Declare `power_do_reset_energy()` | +1 |
| 2 | `src/wb_power.cpp` | Implement `power_do_reset_energy()` (factor from wb_rpc) | +8 |
| 3 | `src/wb_rpc.cpp` | Replace inline reset code in `Wallbox.ResetEnergy` handler with call to `power_do_reset_energy()` | -5 / +1 |
| 4 | `include/wb_mqtt.h` | (no new public function — handler is static) | 0 |
| 5 | `src/wb_mqtt.cpp` | Add `mqtt_cmd_topic[60]` global, `mqtt_cmd_handler()` static function, `mgos_mqtt_sub()` call inside `mqtt_init()` | +45 |
| 6 | `doc/mqtt.md` | New "Commands subscription" section + HA modern YAML example | +60 |
| 7 | Build validation | — | — |
| **Total** | | | **+109 LoC net** |

### V2 Updated Definition of Done

- [ ] `mos build --local` produces a valid fw.zip ≤ 940 KB
- [ ] No new compiler warning
- [ ] Topic subscribed: `wallbox/<id>/cmd` (auto-built from `device.id`)
- [ ] Action `start` → relay ON + immediate state_topic publish
- [ ] Action `stop` → relay OFF + immediate state_topic publish
- [ ] Action `reset_energy` → `power_do_reset_energy()` + immediate state_topic publish
- [ ] Invalid JSON or unknown action → `LOG(LL_WARN, ...)` + drop, no crash
- [ ] `power_do_reset_energy()` is called by both the RPC handler and the MQTT handler (no code duplication)
- [ ] `doc/mqtt.md` documents the cmd contract + complete HA 2024+ YAML `mqtt: → switch:` example ready to paste
- [ ] No regression on existing topics (`announce`, `state`, `system`)
- [ ] No CONNACK hook — `mgos_mqtt_sub()` is called once in `mqtt_init()`

---

## Original V1 plan (kept for traceability — superseded by V2 above)

> The sections below were the original plan submitted to `senior-plan-reviewer`. They are kept verbatim for traceability. Do NOT follow V1 directly: V2 amendments above are authoritative.

## 1. Classification

**Type** : Feature add (additif, pas de breaking change).
**Risk** : Low — code additif. Pas de modification du backend MQTT existant.
**Reversibility** : Excellente — 1 fichier source impacté principalement.

## 2. Objectif (rappel des priorités user)

L'usage quotidien de la wallbox post-L1 se fait via **Home Assistant**, pas via la WebUI. Le user a besoin de :

1. Voir le statut de charge (déjà couvert par L1 : MQTT state topic).
2. **Démarrer / arrêter une charge depuis HA** (✅ déjà possible via `Wallbox.SetRelay` HTTP RPC, mais HA préfère MQTT).
3. **Reset des compteurs d'énergie** depuis HA.

Solution proposée : un nouveau topic MQTT subscribe que la box écoute pour recevoir des commandes JSON.

## 3. Hypothèses

| # | Assumption | Source | Confidence |
|---|---|---|---|
| A1 | La lib `mongoose-os-libs/mqtt` (déjà dans `mos.yml`) supporte `mgos_mqtt_sub()` | API standard Mongoose OS | High |
| A2 | La box ne reçoit les messages MQTT que quand connectée au broker (`mqtt.enable=true`) | Behavior natif Mongoose | High |
| A3 | HA supporte `mqtt switch` avec `state_topic` JSON template + `command_topic` JSON | Doc HA stable | High |
| A4 | JSON `{"action":"..."}` plus extensible que payload brut | Convention IoT | High |
| A5 | `state_topic` du switch HA peut lire `value_json.charging` directement depuis l'existant | Vérifié L1 wb_mqtt.cpp | High |
| A6 | `mgos_mqtt_sub()` accepte des topics dynamiques construits avec sprintf | Doc Mongoose | Medium |
| A7 | Le payload reçu est bien null-terminable via `mg_str` (`p` + `len`) | API Mongoose | Medium |

## 4. Approches alternatives

### Approche A — Topic unique JSON action (RECOMMANDÉE)

```
SUB wallbox/<id>/cmd
Payload: {"action":"start"} | {"action":"stop"} | {"action":"reset_energy"}
```

**Avantages** :
- 1 seul subscribe (économie RAM ESP)
- Extensible : on peut ajouter `{"action":"set_intensity_limit","value":12}` plus tard
- Pattern courant en MQTT (HA, Tasmota, ESPHome)

### Approche B — Topics séparés par action

```
SUB wallbox/<id>/cmd/relay        → "on" / "off"
SUB wallbox/<id>/cmd/reset_energy → trigger
```

**Avantages** :
- Plus simple côté HA (pas de JSON template)
- Messages plus courts

**Inconvénients** :
- N subscribes = N callback handlers
- Moins extensible
- Plus de RAM (1 sub ≈ 50 bytes)

### Approche C — HA Discovery automatique (L3, hors scope)

Publish sur `homeassistant/switch/<id>/config` un message de configuration qui crée automatiquement le switch dans HA. Plus avancé mais nécessite d'avoir le contrôle du namespace `homeassistant/*`. Reporté à L3 selon PLAN.md.

## 5. Comparaison

| Critère | A (JSON unique) | B (topics séparés) | C (HA Discovery) |
|---|---|---|---|
| RAM ESP8266 | Bas | Moyen | Bas (1 sub mais publish startup) |
| Extensibilité | Excellente | Moyenne | Excellente |
| Simplicité config HA | Moyenne (JSON) | Simple | Zéro (auto) |
| LoC firmware | ~57 | ~80 | ~120 |
| Scope L2-A | ✅ | ✅ | ❌ (L3) |

**Recommandation : Approche A.**

## 6. Falsification

| Hypothèse de défaillance | Validité | Mitigation |
|---|---|---|
| Parser JSON Mongoose plante sur message malformé | Possible | `json_scanf` retourne 0 si échec, log warn, drop |
| Lecture out-of-bounds sur payload non null-terminé | Possible | Utiliser `mg_str.p` + `.len` avec `json_scanf(p, len, ...)` |
| Boxes avant L2-A ne reçoivent pas le topic, user pense que ça marche | Confusion possible | `availability_topic` recommandé dans la doc HA + `expire_after` |
| Race condition `Wallbox.SetRelay` HTTP vs MQTT cmd | Théorique | Mongoose event-loop single-threaded, pas de race |
| `mqtt.enable=false` → user perd contrôle | Par design | Fallback HTTP RPC reste dispo |
| Topic non subscribed si MQTT n'est pas encore connecté au boot | Probable | Implémenter dans `MGOS_EV_MQTT_CONNACK` event handler, pas dans `mqtt_init()` directement |
| Payload `{"action":"start"}` sans guillemets autour de `start` | Frozen tolérant ? | Documenter strict JSON dans doc/mqtt.md |

## 7. Recommandation finale

**Approche A — Topic unique `wallbox/<id>/cmd` avec JSON `{"action":"..."}`.**

Decisions actées par user :
- Topic name : `wallbox/<id>/cmd`
- Live data refresh : à décider en L2-B (peu d'impact sur L2-A)
- Style WebUI : moderne minimal (impact L2-B uniquement)

## 8. Plan d'implémentation

| Étape | Fichier | Action | LoC |
|---|---|---|---|
| 1 | `include/wb_mqtt.h` | Déclarer `mqtt_subscribe_cmd()` | +1 |
| 2 | `src/wb_mqtt.cpp` | Add `mqtt_cmd_topic` global + `mqtt_subscribe_cmd()` | +15 |
| 3 | `src/wb_mqtt.cpp` | Handler `mqtt_cmd_handler()` parses `{"action":"..."}` | +30 |
| 4 | `src/wb_mqtt.cpp` | Hook subscription on `MG_EV_MQTT_CONNACK` event (subscribes after MQTT connected, not at init) | +5 |
| 5 | `src/wb_mqtt.cpp` | `power_reset_energy()` + zero meter.* (factor common code with `Wallbox.ResetEnergy`) | +5 |
| 6 | `doc/mqtt.md` | Section "Commands subscription" with HA example | +60 |
| 7 | Build de validation | — | — |
| **Total** | | | **+116 LoC** |

## 9. Critères de succès (Definition of Done — L2-A)

- [ ] `mos build --local` produit un fw.zip valide
- [ ] Le firmware compile sans warning nouveau
- [ ] Topic subscribe : `wallbox/<id>/cmd` (avec `<id>` = `device.id`)
- [ ] Action `start` → relais ON
- [ ] Action `stop` → relais OFF
- [ ] Action `reset_energy` → équivalent `Wallbox.ResetEnergy` RPC
- [ ] Payload invalide ou action inconnue → log warning + drop (pas de crash)
- [ ] `doc/mqtt.md` documente le contrat + exemple HA `mqtt switch` prêt à coller
- [ ] `wallbox/<id>/state.charging` reflète bien l'état physique du relais (déjà OK depuis L1)
- [ ] Pas de breaking change sur les topics state/system/announce existants
- [ ] Build de référence après commit : taille fw.zip ≤ 940 KB (alerte sinon)

## 10. Out of scope (= L2-B et L3)

- WebUI moderne — L2-B
- HA MQTT Discovery (auto-création switch dans HA) — L3
- Setting de la limite d'intensité via MQTT — L3
- Détection de charge VE améliorée — L3
- Protections temp/courant — L3

## 11. Décisions ouvertes (à valider avant impl)

**Q1** — Faut-il publier un message **retained** quand la commande est exécutée ? (par exemple `wallbox/<id>/cmd/ack` avec `{"action":"start","status":"ok"}`)
- (a) Non — la valeur de retour est implicite via `state_topic` qui change après ~60s → simple
- (b) Oui sur un topic ack — feedback rapide pour HA, mais +complexité
- **Reco** : (a) — minimaliste, le state_topic `charging` se met à jour rapidement (60s max après le tick suivant de `mqtt_send_state_topic`)

**Q2** — Si MQTT se déconnecte temporairement, la subscription est-elle restaurée automatiquement ?
- Mongoose OS gère la reconnexion automatiquement, mais le subscribe est-il re-joué ?
- **Reco** : Subscribe sur l'event `MG_EV_MQTT_CONNACK` (re-fired à chaque reconnexion) → garantit le re-sub. À implémenter explicitement.

**Q3** — Quel **QoS** pour le subscribe ?
- (a) QoS 0 — pas de guarantee, mais simple et léger (cohérent avec les publish actuels)
- (b) QoS 1 — at-least-once delivery, plus sûr pour des commandes
- **Reco** : (b) QoS 1 pour `cmd` — c'est une commande, on veut la guarantee. Les `state` restent en QoS 0 (statut perdable, ré-envoyé toutes les 60s).

**Q4** — Faut-il un **mécanisme d'authentification** sur ce topic ?
- (a) Non — le broker MQTT a déjà des credentials (user/pass dans `mos.yml`). Si quelqu'un peut publier sur le broker, il a déjà accès à ta box.
- (b) Oui — payload doit contenir un secret pré-partagé.
- **Reco** : (a) — sécurité gérée au niveau du broker. Cohérent avec ESPHome/Tasmota.
