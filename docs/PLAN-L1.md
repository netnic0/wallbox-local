# Livraison 1 — Plan détaillé (refactor backend, suppression OCPP)

> **Status: V2 AMENDED — APPROVED 2026-06-04**
> Reviewed by `senior-plan-reviewer` agent (confidence 89%, APPROVED WITH AMENDMENTS).
> Validated by user 2026-06-04 with the 5 mandatory amendments below.

## V2 Amendments (binding for implementation)

The 5 review amendments and 3 user decisions below **override** the corresponding sections of the original V1 plan further down. When in doubt, the V2 amendments are authoritative.

### Amendments from review

1. **A1 — `power_update()` MUST be called from `process_loop()`** (currently invoked indirectly via `ocpp_synchronize() → power_update()` at `wb_ocpp.cpp:248`). Without this migration, the energy counter would be permanently frozen. Step 5 must add the explicit `power_update()` call to `process_loop()` in `main.cpp` AFTER removing `ocpp_synchronize()`. Tableau LoC corrigé en `+3 / -1`.

2. **A2 — `ocpp_reset_hard()` is used in 2 NON-OCPP RPC handlers** (`wb_rpc.cpp:102` for `Wallbox.Reboot` and `wb_rpc.cpp:117` for `Wallbox.Reset`). These two calls must be replaced by a direct `mgos_system_restart_after(10000)` BEFORE deleting `wb_ocpp.cpp` at step 8 — otherwise the build breaks. This is now Step 6 in the V2 ordering.

3. **A3 — Do NOT remove the `mongoose` lib (variant esp8266) from `mos.yml` in L1.** The `rpc-ws` lib depends transitively on it. Only remove `ota-http-client` (which is genuinely OCPP-only). Step 8 list of libs to remove is reduced to `ota-http-client` only.

4. **A4 — Energy migration strategy: option (b) — start from zero.** The HA recorder already keeps the historical Wh values, so resetting the firmware-side cumulative counter at OTA upgrade has no functional impact for the user. No migration code is needed in `main.cpp`. The `meter.*` namespace starts empty.

5. **A5 — Reformulate the success criterion `grep -ri "ocpp" src/ include/ mos.yml = 0 results`.** With option (b) chosen, no migration code references `ocpp.*`, so the criterion can stay as a hard zero. However, the `migrate_config()` function currently references `ocpp.url` and `ocpp.name` — it must be deleted (or rewritten) at step 7 since `conf_version` already passed 2 on existing deployments. Updated criterion: `0 results in src/ include/ mos.yml`.

### User decisions (ratified 2026-06-04)

- **Q1 = (b)** Start fresh — no firmware-side migration of `ocpp.transaction.consumption` to `meter.total_energy`. HA recorder is the source of truth for historical energy.
- **Q2 = (c)** `connected: true` constant in MQTT state JSON (sentinel of presence for HA `expire_after: 180`).
- **Q3 = (a)** Keep `tid: 0` hardcoded in MQTT state JSON for HA backward-compat.
- **Q4** = invalid (skipped). API HLW8012 already validated by the review: `mgos_hlw8012_readVoltage()` returns `unsigned int`, `mgos_hlw8012_readCurrent()` returns `double`. Both exist in `deps/mongoose-lib-hlw8012/include/mgos_hlw8012.h`.

### V2 Step ordering (overrides V1 §8)

| V2 Step | Action | Files | LoC ±  | Build OK after |
|---|---|---|---|---|
| 1 | Add `meter.*` namespace | `mos.yml` | +6 / 0 | yes (config additive) |
| 2 | Add `power_read_voltage()` + `power_read_current()` | `wb_power.h/cpp` | +14 / 0 | yes |
| 3 | Add RPCs `Wallbox.SetRelay`, `Wallbox.ResetEnergy` | `wb_rpc.h/cpp` | +60 / 0 | yes |
| 4 | Extend `MQTT_STATE` JSON with `power`, `voltage`, `current` | `wb_mqtt.cpp` | +12 / 0 | yes |
| 5 | Migrate `power_update()` into `process_loop`; refactor `power_update` to write `meter.*` | `main.cpp`, `wb_power.cpp` | +20 / -10 | yes |
| 6 | Replace `ocpp_reset_hard()` in 2 RPC handlers; substitute `ocpp_is_connected` etc. | `wb_rpc.cpp`, `wb_mqtt.cpp` | +5 / -8 | yes |
| 7 | Clean `main.cpp` (remove `ocpp_synchronize`, `ocpp_connect_backend`, OCPP boot logic, delete `migrate_config`) | `main.cpp` | +5 / -25 | yes |
| 8 | Delete `wb_ocpp.cpp` + `include/wb_ocpp.h`. Remove `ota-http-client` from `mos.yml`. Remove OCPP `config_schema` entries. Bump `version: 1.0.0`. | repo | 0 / -899 | **final** |
| 9 | Build final + verify DoD | — | — | — |

### V2 Definition of Done (overrides V1 §9)

- [ ] `mos build --local` produces a `build/fw.zip` < 1 MB.
- [ ] `manifest.name == Wallbox-Shelly1PM`, `version == 1.0.0`.
- [ ] `wb_ocpp.cpp` and `include/wb_ocpp.h` are deleted.
- [ ] `grep -ri "ocpp" src/ include/ mos.yml` returns **0 results** (no exception clause — Q1=(b) eliminates the migration code path).
- [ ] No reference to `ocpp_is_connected`, `ocpp_synchronize`, `ocpp_connect_backend`, `ocpp_reset_hard` anywhere in `src/`.
- [ ] MQTT topic `wallbox/<id>/state` JSON contains: `uptime`, `connected`, `charging`, `energy`, `intensity`, `tid`, `temperature`, **+** `power`, `voltage`, `current`. Field `connected` is always `true`. Field `tid` is always `0`.
- [ ] RPCs `Wallbox.SetRelay {on:bool}` and `Wallbox.ResetEnergy` respond and behave correctly (manual smoke test on box).
- [ ] HA dashboard continues to receive valid readings on `connected`, `charging`, `energy`, `intensity`, `tid`, `temperature` (backward compat preserved).
- [ ] Each commit on the implementation branch corresponds to one V2 step and builds independently.

---

## Original V1 plan (kept for traceability — superseded by V2 above)

> The sections below were the original plan submitted to `senior-plan-reviewer`. They are kept verbatim for traceability. Do NOT follow V1 directly: V2 amendments above are authoritative.

## 1. Classification

**Type** : Refactor — suppression d'un sous-système entier (OCPP, 844 LoC) + extension légère MQTT/RPC.
**Risk** : Medium-High — touche le namespace de configuration (persisté en flash) et la boucle principale. Mauvais refactor → wallbox briquée (pas de WebUI fonctionnelle, MQTT cassé, OTA cassé).
**Reversibility** : Moyenne — `fw (7).zip` reste un rollback possible via OTA.

## 2. Assumptions

| # | Assumption | Source | Confidence | Si fausse |
|---|---|---|---|---|
| A1 | Le dashboard HA actuel ne consomme **que** `wallbox/<id>/state` et `wallbox/<id>/system`. Pas de `/announce` après le 1er run. | `doc/mqtt.md` + PLAN.md | High | Rien à faire — les 3 topics sont préservés |
| A2 | Le user n'utilise **plus** OCPP (pas de backend Open e-Mobility configuré). | PLAN.md ("OCPP supprimé") | High | Sinon il aurait gardé le repo upstream |
| A3 | `mgos.yml` accepte de retirer les libs `ota-http-client`, `mongoose` (variant esp8266), `ellavas/provision` côté **OCPP-only**, sans casser la base. | `mos.yml` lecture | **Medium** | Build échoue ou OTA cassé. Mitigation : conserver toutes les libs au L1, scope limité aux fichiers `.cpp/.h` |
| A4 | Le namespace de config `ocpp.transaction.*` peut rester en place sur les boxes existantes (compat config flash) tant qu'on n'y écrit plus. On crée un nouveau namespace `meter.*`. | Mongoose OS config schema (additif) | **Medium** | Config corrompue → reset usine |
| A5 | `power_update()` est essentiel (sinon pas d'énergie). Il doit être appelé périodiquement (toutes 60s suffisent — c'est ce que faisait le code OCPP). | grep wb_ocpp.cpp:248,435 | High | Énergie figée à 0 |
| A6 | `mgos_hlw8012_readVoltage` et `readCurrent` existent dans la lib `ellavas/mongoose-lib-hlw8012`. | API similaire au lib upstream `mgos_hlw8012_readActivePower` | **Low** | Si absent → fallback : `voltage = power / current` ou valeurs nulles + log |
| A7 | La WebUI 0.7.7 buildée (qui sera remplacée à L2) tolère des endpoints OCPP retournant 404 ou des champs absents dans `Wallbox.GetInfo`. | À vérifier visuellement après flash | **Low** | Affichage cassé temporaire (acceptable, sera fixé à L2) |
| A8 | Pas de signature OTA → notre fw.zip uploadable. | PLAN.md, vérifié | High | — |
| A9 | App name `Wallbox-Shelly1PM` doit rester pour OTA (la box rejette si différent). | PLAN.md | High | OTA refuse l'upload |

**Assumptions Low/Medium à valider** : A3, A4, A6, A7. Stratégie : **clarifier A6 par lecture rapide de `deps/hlw8012/include/mgos_hlw8012.h`** avant d'implémenter, et **adopter une approche additive plutôt que destructive sur la config (A4)**.

## 3. Approches alternatives

### Approche A — "Nettoyage chirurgical minimal" (RECOMMANDÉE)

**Principe** : suppression progressive et **additive d'abord, destructive ensuite**.

1. Ajouter le namespace de config `meter.*` (sans toucher `ocpp.*`).
2. Ajouter dans `wb_power.h/cpp` les fonctions `power_read_voltage()` et `power_read_current()`.
3. Ajouter dans `wb_rpc.cpp` les RPCs `Wallbox.SetRelay` et `Wallbox.ResetEnergy`.
4. Modifier `wb_mqtt.cpp` pour publier les nouveaux champs `power`, `voltage`, `current` (en plus des existants).
5. Modifier `main.cpp` pour appeler `power_update()` dans `process_loop` et retirer `ocpp_synchronize()`, `ocpp_connect_backend()`.
6. **Seulement à la fin** : supprimer `wb_ocpp.cpp` + `wb_ocpp.h`, retirer `#include "wb_ocpp.h"` partout, retirer libs OCPP-only de `mos.yml`, bumper `version: 1.0.0`.
7. Remplacer les références `mgos_sys_config_get_ocpp_transaction_consumption()` par `mgos_sys_config_get_meter_total_energy()` (et similaires) dans `wb_mqtt.cpp`, `wb_rpc.cpp`, `main.cpp`.
8. Garder `connected` = `wifi_connected` (substituer `ocpp_is_connected()`).
9. Garder `tid: 0` en dur (ou retirer si HA discovery l'exige plus tard, mais L1 préserve le contrat MQTT).

**Avantages** : à chaque étape (1-5), le projet builde et fonctionne. Bissection facile en cas de régression.

### Approche B — "Big Bang"

Supprimer `wb_ocpp.cpp/h` et tous les `#include`/appels d'un coup. Réécrire `main.cpp`, `wb_mqtt.cpp`, `wb_power.cpp`, `wb_rpc.cpp` en une seule passe.

**Avantages** : code plus propre dès le début (pas de doublons temporaires).
**Inconvénients** : N points de cassure simultanés, debug pénible si build échoue ou tests négatifs sur la box.

### Approche C — "Fork wb_ocpp.cpp en wb_meter.cpp"

Renommer `wb_ocpp.cpp` → `wb_meter.cpp`, garder uniquement les fonctions liées à l'énergie (`power_update` callers, transitions transaction-like).

**Avantages** : conserve la logique d'agrégation existante.
**Inconvénients** : porte une dette d'OCPP en faux-nez. PLAN.md dit explicitement "supprimer wb_ocpp.cpp" → désaligné avec l'intention.

## 4. Comparaison

| Critère | A (chirurgical) | B (big bang) | C (fork) |
|---|---|---|---|
| Performance fw final | = | = | = |
| Complexité refactor | **Basse** (étapes auto-vérifiables) | Haute | Moyenne |
| Maintainabilité fin L1 | Bonne | Bonne | Mauvaise (legacy déguisé) |
| Testability incrémentale | **Excellente** | Faible | Bonne |
| Risk de brique | **Bas** | Élevé | Bas |
| Estimation temps | 4 h | 3 h (si tout marche du 1er coup, peu probable) | 5 h |

**Recommandation : Approche A.**

## 5. Falsification

Tentatives de réfuter A :

| Hypothèse de défaillance | Validité | Mitigation |
|---|---|---|
| « Étape 1 (ajout namespace `meter.*`) casse le bootloader car > taille config max » | Improbable — namespace additif, ~6 entrées int/bool | Build de validation après chaque étape |
| « Conserver `ocpp.transaction.*` en parallèle gaspille de la flash » | Vrai mais marginal (100 octets) | Supprimer le namespace en L2 ou L3, après migration |
| « `power_update()` appelé toutes 60s n'est pas assez fréquent → intensity moyenne fausse » | Code actuel utilise déjà ce timing (60s loop) → pas de régression | — |
| « Substituer `connected = ocpp_is_connected()` par `connected = mqtt_is_connected()` casse la sémantique HA » | Plausible : HA voit la box comme "déconnectée" pendant les 30s de boot. Mais c'est plus honnête (la box est vraiment "connectée à HA" via MQTT) | Doc à mettre à jour, et c'est ce que le user demande implicitement |
| « Approche A laisse temporairement le projet dans un état hybride pollué » | Vrai pour quelques heures | Acceptable — chaque commit reste atomique |
| « `mgos_hlw8012_readVoltage/readCurrent` n'existent pas (A6) » | Possible | Spike avant code : 5 min pour vérifier `deps/hlw8012/include/mgos_hlw8012.h` |

**Conclusion** : l'approche A résiste aux tentatives de falsification. Le seul risque réel est A6 (API HLW8012) — mitigé par un spike de 5 min en début de L1.

## 6. Critique honnête

- **Risque résiduel** : la config flash existante des boxes en production contient `ocpp.transaction.consumption` (le total d'énergie cumulé !). Migrer vers `meter.total_energy` veut dire **soit** lire l'ancienne valeur dans `mgos_app_init` et l'écrire dans la nouvelle **soit** repartir de 0. Décision à prendre avec l'utilisateur.
- **Style** : le code original utilise camelCase pour les JSON keys (`heapSize`, `freeHeapSize`). Pour cohérence, les nouveaux champs `power/voltage/current` doivent être en camelCase aussi (et non `total_energy` qui devrait s'écrire `totalEnergy`).
- **Observabilité** : aucun champ `firmware` ou `built_at` dans `state` actuellement. À garder pour L1 (pas le scope) mais à suggérer pour L3.
- **Nom de version 0.7.7 → 1.0.0** : saut major justifié (suppression OCPP = breaking pour OCPP backend). OK.

## 7. Recommandation finale

**Approche A — Nettoyage chirurgical minimal**, en 7 étapes auto-vérifiables (build + smoke test entre chaque), avec :

- Spike A6 (API HLW8012) **avant** d'écrire le moindre code.
- Décision préalable user sur la **migration de la valeur cumulée d'énergie** (lire ancien namespace au boot, ou repartir de 0).
- Garde-fou : **chaque étape doit produire un fw.zip qui builde proprement**. Si build casse, rollback git stash et on re-discute.

## 8. Mapping changements (Approche A)

| Étape | Fichiers touchés | LoC ajoutées | LoC supprimées | Durée estimée |
|---|---|---|---|---|
| 0. Spike API HLW8012 | (lecture) | 0 | 0 | 5 min |
| 1. Ajout namespace `meter.*` dans `mos.yml` | `mos.yml` | +6 | 0 | 5 min |
| 2. Ajout `power_read_voltage/current` | `wb_power.h/cpp` | +14 | 0 | 10 min |
| 3. Ajout RPCs `SetRelay`, `ResetEnergy` | `wb_rpc.h/cpp` | +60 | 0 | 30 min |
| 4. Étendre `MQTT_STATE` JSON | `wb_mqtt.cpp` | +12 | 0 | 15 min |
| 5. `process_loop` appelle `power_update()` | `main.cpp` | +1 | -1 | 5 min |
| 6. Migration energie au boot | `main.cpp` | +15 | 0 | 20 min |
| 7. Substitutions `ocpp_*` → `meter_*` / `mqtt_is_connected` | `wb_mqtt.cpp`, `wb_rpc.cpp`, `main.cpp` | +5 | -8 | 30 min |
| 8. Suppression `wb_ocpp.*`, retrait des libs OCPP-only de `mos.yml`, bump `version: 1.0.0` | repo | 0 | -899 | 10 min |
| 9. Build final + check fw.zip | — | — | — | 15 min |
| **Total estimé** | | **+113** | **-908** | **~2h30** |

(L'estimation PLAN.md de 4-5h reste cohérente avec les imprévus.)

## 9. Critères de succès (Definition of Done — L1)

- [ ] `mos build --local` produit un `build/fw.zip` < 1 MB.
- [ ] `manifest.name == Wallbox-Shelly1PM`, `version == 1.0.0`.
- [ ] `wb_ocpp.cpp` et `include/wb_ocpp.h` supprimés.
- [ ] `grep -ri "ocpp" src/ include/ mos.yml` → 0 résultats (sauf `migrate_config` éventuelle).
- [ ] Aucune référence à `ocpp_is_connected`, `ocpp_synchronize`, `ocpp_connect_backend` dans `src/`.
- [ ] Topics MQTT `wallbox/<id>/state` JSON contient : `uptime`, `connected`, `charging`, `energy`, `intensity`, `tid`, `temperature`, **+** `power`, `voltage`, `current`.
- [ ] RPCs `Wallbox.SetRelay {on:bool}` et `Wallbox.ResetEnergy` répondent.
- [ ] HA dashboard existant continue à voir `connected`, `charging`, `energy`, `intensity`, `tid`, `temperature` (compat backwards garantie).

## 10. Out of scope (= Livraisons 2 et 3)

- WebUI moderne vanilla JS — **L2**
- HA MQTT Discovery — **L3**
- Détection charge VE améliorée (`charging = relais_on AND power > 100W`) — **L3**
- Protections temp/courant (reboot > 80°C) — **L3**

## 11. Décisions ouvertes pour l'utilisateur (avant approbation)

**Q1 — Migration énergie** : la box actuelle contient un compteur d'énergie cumulé dans `ocpp.transaction.consumption`. À l'OTA L1 :
  - **(a)** Lire l'ancienne valeur au boot et la copier dans `meter.total_energy`. Reprise de l'historique.
  - **(b)** Repartir de 0. Plus simple. L'historique d'énergie est perdu (mais HA a le sien dans son recorder).
  - **(c)** Ne pas trancher maintenant, garder un timer de migration optionnel (lit l'ancien si `meter.total_energy == 0` au boot et `ocpp.transaction.consumption > 0`).

**Q2 — Champ `connected` dans MQTT state** : sémantique :
  - **(a)** `mqtt_is_connected()` (toujours `true` quand le state est publié, peu informatif)
  - **(b)** `wifi_is_connected()` (informatif mais redondant)
  - **(c)** Toujours `true` (sentinelle de présence — la box est "up" si elle publie)
  - **(d)** Retirer le champ (mais casse la rétro-compat HA)

**Q3 — `tid` dans MQTT state** : maintenant qu'OCPP est mort :
  - **(a)** Garder `tid: 0` en dur (rétro-compat HA)
  - **(b)** Retirer (casse rétro-compat)

**Q4 — Spike avant code** : OK pour 5 min de spike sur l'API HLW8012 ?
