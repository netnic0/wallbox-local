# Wallbox-Local — Plan d'implémentation

> **Status au 2026-06-04 16:30** : Livraison 1 ✅ TERMINÉE (tag `v1.0.0`). Livraison 2-A ✅ TERMINÉE (MQTT cmd topic + doc HA 2024+). Push sur `github.com/netnic0/wallbox-local`. **EN PAUSE — reprise quand tu veux**. Reste à faire : flash test sur la box + L2-B (WebUI moderne) + L3 (HA Discovery + sécurité).

## Contexte

- Matériel : **Shelly 1PM Gen1** (ESP8266 + BL0937)
- Firmware actuel : `Wallbox-Shelly1PM 0.3.0` (Mongoose OS), reçu depuis Open e-Mobility / SAP Labs France
- Fichier de référence : `C:\Users\I058304\Downloads\fw (7).zip` (ne PAS uploader, vieille version)
- Repo source clone : `C:\Users\I058304\Downloads\shelly-ocpp-wallbox\` (Apache 2.0, version 0.7.7)
- Repo extracted (analyse fw.zip) : `C:\Users\I058304\Downloads\fw_extracted\Wallbox-Shelly1PM-0.3.0\`
- WebUI actuelle accessible via `http://wallbox.local` ou `http://192.168.1.123`

## Décisions prises

| Sujet | Choix |
|---|---|
| Découpage | **🅱 — 3 livraisons incrémentales** (chacune = 1 fw.zip uploadable) |
| App name fw | **Garder `Wallbox-Shelly1PM`** (compat OTA) |
| Topics MQTT | **Inchangés** (`wallbox/<id>/state`, `/system`, `/announce`) |
| Champs JSON state | Ajouter `power`, `voltage`, `current`. Garder `energy`, `connected`, `charging`, `tid` (=0), `intensity`, `uptime`, `temperature` |
| Compteurs énergie | **Session courante + Total cumulé** (2 compteurs) |
| HA MQTT Discovery | **OUI** (livraison 3) |
| Détection charge VE | **OUI** : `charging = relais_on AND power > 100W` (livraison 3) |
| Protections | **OUI** : temp > 80°C → relais OFF + reboot. Courant > 12A pendant 5s → relais OFF (livraison 3) |
| WebUI auth | **Optionnelle, OFF par défaut** |
| WebUI framework | **Vanilla JS pur** (cible <30KB gzippés) |
| Live data WebUI | **WebSocket** sur `/rpc` Mongoose |
| Toolchain | **WSL2 Ubuntu 26.04** (déjà installée) — option **(c)** validée |
| GitHub | **Garder ouvert jusqu'au 1er build OK**, puis couper |
| License | Apache 2.0 conservée |
| Versions | v1.0.0 (L1), v1.1.0 (L2), v1.2.0 (L3) |

## Architecture cible

```
src/
  main.cpp        — init + boucle 60s (sans OCPP)
  wb_mqtt.cpp     — publi MQTT autonome (vraies valeurs depuis BL0937)
  wb_power.cpp    — étendu : expose readVoltage, readCurrent + compteur session
  wb_rpc.cpp      — RPC simplifié + nouveaux endpoints SetRelay/ResetEnergy
  wb_thermistor.cpp — inchangé
  wb_util.cpp     — inchangé
  wb_safety.cpp   — NOUVEAU (livraison 3) : protections temp/courant
  wb_discovery.cpp — NOUVEAU (livraison 3) : HA MQTT discovery

include/
  wb_mqtt.h, wb_power.h, wb_rpc.h, wb_thermistor.h, wb_util.h
  wb_safety.h, wb_discovery.h (livraison 3)

# SUPPRIMÉS :
src/wb_ocpp.cpp (844 lignes), include/wb_ocpp.h

www/
  index.html (squelette, ~3 KB)
  app.js (logique vanilla, ~8 KB)
  style.css (CSS moderne, ~6 KB)
```

## État actuel — Phase 0bis (toolchain)

✅ **Source du repo cloné** dans `C:\Users\I058304\Downloads\shelly-ocpp-wallbox\` (master HEAD = 58c3691)

✅ **WSL2 Ubuntu 26.04 LTS** opérationnelle (`wsl -d Ubuntu`)
   - git ✅ `/usr/bin/git`
   - curl ✅ `/usr/bin/curl`

❌ **Docker** : pas installé dans WSL Ubuntu (à installer)
❌ **mos CLI** : pas installé (à installer)

## Reprise demain — Étapes à faire dans l'ordre

### 1. Phase 0bis (continuer) — Setup toolchain dans WSL Ubuntu

**Stratégie A — Docker (recommandée, reproductible)** :
```bash
# Dans WSL Ubuntu
wsl -d Ubuntu

# Installer Docker
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
# (relancer la session WSL après)

# Vérifier image build Mongoose OS
docker pull mgos/esp8266-build:2.17.0-1.5.0-r5
# Si l'image n'existe plus, tester variantes :
# - mgos/esp8266-build:latest
# - mgos/mos:latest
```

**Stratégie B — mos CLI natif Linux** :
```bash
wsl -d Ubuntu
sudo apt update
sudo apt install -y software-properties-common ca-certificates
sudo add-apt-repository ppa:mongoose-os/mos
sudo apt install -y mos

# Tester
mos --version
```

→ **Si la PPA ou Docker hub a expiré l'image, on a un problème majeur** (Mongoose OS abandonné depuis ~2024). Plan B : essayer `mos-latest` ou compiler from source. Plan C : abandonner Mongoose OS pour Tasmota (mais flash série requis).

### 2. 1er build de validation (sans modification)

```bash
cd /mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox

# Avec Docker :
docker run --rm -v $(pwd):/app mgos/esp8266-build:2.17.0-1.5.0-r5 mos build --local --platform esp8266

# Ou avec mos CLI natif :
npm install
mos build --local --platform esp8266
```

**Critère succès** : un `build/fw.zip` produit, contenu similaire au `fw (7).zip` actuel.

### 3. Couper l'accès GitHub (après build OK)

Une fois les libs Mongoose OS pullées en cache local (`~/.mos/...` dans WSL), on peut :
- révoquer le token gh : `gh auth logout`
- révoquer dans https://github.com/settings/tokens

### 4. Livraison 1 — Backend propre (4-5 h)

Refactor selon le plan détaillé dans la section "Livraison 1" du dialogue (voir `1a10a98d-526a-4ed7-96d7-36a741d1e208.jsonl`).

Fichiers à modifier :
- `mos.yml` (virer libs OCPP, version 1.0.0)
- `src/main.cpp` (virer appels ocpp_*)
- `src/wb_mqtt.cpp` (sources réelles BL0937)
- `src/wb_power.cpp` (exposer voltage/current)
- `src/wb_rpc.cpp` (ajouter SetRelay, ResetEnergy)
- Supprimer `src/wb_ocpp.cpp` + `include/wb_ocpp.h`

### 5. Livraison 2 — WebUI moderne (4-6 h)

- Réécrire `www/index.html`, `www/app.js`, `www/style.css` en vanilla
- Simplifier `package.json` (juste minif + gzip)

### 6. Livraison 3 — Polish (1-2 h)

- HA MQTT Discovery (`wb_discovery.cpp`)
- Détection charge VE améliorée
- Protections temp/courant (`wb_safety.cpp`)
- Doc finale

## Points de vigilance

- **App name doit rester `Wallbox-Shelly1PM`** dans `mos.yml` pour que l'OTA accepte le fichier
- **Topics MQTT inchangés** pour ne pas casser le dashboard HA actuel
- **Calibration BL0937** : garder les multiplicateurs actuels (`current=25.7400, voltage=313.4000, power=3414.2900`)
- **Pas de signature OTA** dans le firmware actuel (vérifié) → notre fw.zip sera accepté
- **Backup** : le `fw (7).zip` original est gardé dans `Downloads/` au cas où rollback manuel nécessaire

## Comment reprendre demain

## 📍 État au 2026-06-04 16:30 — PAUSE EN COURS

### ✅ Ce qui est terminé

**Phase 0bis — Toolchain** (build local Docker + Mongoose OS) ✅
- WSL Ubuntu 26.04 + Docker 29.5.3 + image `mgos/esp8266-build:2.2.1-1.5.0-r5` + `mgos/mos:latest`
- Note : tag Docker `2.17.0-1.5.0-r5` du PLAN initial **n'existe pas** sur Docker Hub. On utilise `2.2.1-1.5.0-r5`.
- Serveurs Mongoose OS (mongoose-os.com/downloads) **down (404)** → seul `mos build --local` fonctionne.

**Livraison 1 — Backend cleanup** ✅ (taggé `v1.0.0`)
- 13 commits, 940 LoC supprimées net, OCPP entièrement retiré, namespace `meter.*` créé, RPCs `Wallbox.SetRelay`/`Wallbox.ResetEnergy` ajoutés, MQTT enrichi (power/voltage/current), 5 amendements review intégrés
- Tag : `v1.0.0` → commit `d0973c1` (build clean OK from scratch)
- Détails dans `docs/PLAN-L1.md` (V2 amended)

**Livraison 2-A — MQTT command topic + doc HA** ✅
- 2 commits (`6e588e3` feature + `e9e402c` review fix)
- Topic `wallbox/<id>/cmd` ajouté → la box accepte `{"action":"start"|"stop"|"reset_energy"}`
- 4 amendements review intégrés (no double-sub, immediate state publish, helper factor, HA 2024+ syntax)
- Doc `doc/mqtt.md` réécrit avec contrat MQTT complet + exemple HA `mqtt: → switch:` prêt à coller
- Détails dans `docs/PLAN-L2-A.md` (V2 amended)

**Repo GitHub** ✅
- `https://github.com/netnic0/wallbox-local` — public, branche `main`
- Auth : `gh` CLI installé dans WSL, account `netnic0` connecté
- Identité git locale : `netnic0 <nicolas.diguet@gmail.com>`
- Remote upstream `sebastien-savalle/shelly-ocpp-wallbox` retiré, tag local `upstream-58c3691` conservé pour rollback éventuel

### 📜 Commande de build de référence (à mémoriser)

```bash
cd /mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox
docker run --rm \
  --entrypoint /bin/sh \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v /mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox:/mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox \
  -w /mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox \
  mgos/mos:latest \
  -c 'git config --global --add safe.directory "*" && mos build --local --platform esp8266'
# Sortie: build/fw.zip (~924 KB)

# Vérifier le fw produit:
python3 scripts/compare_fw.py [build/fw.zip] [reference.zip]
```

### 📌 Reste à faire — choisis l'ordre à la reprise

**Option 1 — Flash + test runtime** (le plus utile pour valider)
1. Flasher le `build/fw.zip` actuel (= L1 v1.0.0 + L2-A) sur la box via OTA
   ```bash
   curl -v -F file=@build/fw.zip http://wallbox.local/update
   ```
2. Vérifier après reboot :
   - HA reçoit toujours les topics `wallbox/<id>/state` et `/system`
   - Les nouveaux champs `power`, `voltage`, `current` arrivent dans les sensors
   - `mosquitto_pub -t wallbox/<id>/cmd -m '{"action":"start"}'` allume le relais
   - `mosquitto_pub -t wallbox/<id>/cmd -m '{"action":"stop"}'` éteint le relais
3. Configurer HA : copier-coller le YAML de `doc/mqtt.md` dans `configuration.yaml`
4. Si problème, rollback OTA possible avec `fw (7).zip` original (v0.3.0 OCPP)

**Option 2 — Livraison 2-B : WebUI moderne** (~3-4 h estimées)
- Refonte `www/index.html`, `www/app.js`, `www/style.css` en vanilla JS pur (cible <30 KB gzipés)
- Bloc hero live (puissance + jauge SVG) + bouton ON/OFF principal
- Sections collapsibles (Wi-Fi, MQTT, Firmware, Admin) — suppression section OCPP cassée
- WebSocket sur `ws://wallbox.local/rpc` ou polling 2s pour live data (à décider)
- Refactor `package.json` : retirer webpack/babel/axios → garder juste minif + gzip
- Style : Moderne minimal (décidé)
- Actuellement la WebUI 0.7.7 toujours présente affiche des champs OCPP cassés (champs absents dans `Wallbox.GetInfo` post-L1) → c'est pourquoi L2-B la réécrit.

**Option 3 — Livraison 3 : Polish** (~1-2 h estimées)
- HA MQTT Discovery (auto-création des entités HA)
- Détection charge VE améliorée : `charging = relais_on AND power > 100W`
- Protections temp (>80°C → coupe relais + reboot) + courant (>12A pendant 5s → coupe relais)
- Nouveau fichier `src/wb_safety.cpp` + `src/wb_discovery.cpp`
- Doc finale + release notes

**Option 4 — Cleanup technique** (mineur, à faire en passant)
- Convertir les 4 `sprintf` de topics MQTT en `snprintf` (sécurité défensive contre `device.id` longs) — flagged par code review L2-A
- Optimiser le parser action MQTT : remplacer `json_scanf %Q` (alloc heap) par `strncmp` direct sur le payload (économie RAM/cycles)

### 🎯 Recommandation à la reprise

Tu m'as dit dans cette session : *"l'objectif #1 c'est que HA via MQTT marche bien"*. Donc **Option 1 (flash + test HA)** est la plus utile maintenant — elle valide TOUT le travail des sessions précédentes. Si quelque chose marche pas, on debug avant de continuer. Si tout marche, on a la liberté de choisir L2-B / L3 sans pression.

### 💾 Memory dispo pour reprise

- `~/.claude/projects/C--Users-I058304/memory/project_wallbox-local-firmware.md` (à mettre à jour avec L1+L2-A done)
- Repo : `C:\Users\I058304\Downloads\shelly-ocpp-wallbox` — branche `main` HEAD `e9e402c`
- Tag : `v1.0.0` → `d0973c1`

---

## 🔁 Comment reprendre

À la prochaine session, dis-moi simplement **une** de ces phrases :

- **"Reprends le projet wallbox depuis PLAN.md"** → je relis le PLAN et je te demande quelle option tu veux attaquer (1 / 2 / 3 / 4)

- **"Wallbox : on flashe la box"** → je te guide pour flasher `build/fw.zip` via OTA et tester avec HA (Option 1)

- **"Wallbox : on attaque L2-B"** → je rédige le plan WebUI L2-B, je le fais reviewer par senior-plan-reviewer, et on attaque (Option 2)

- **"Wallbox : on attaque L3"** → on saute la WebUI et on fait le polish HA Discovery + safety (Option 3)

- **"Wallbox : flash et raconte ce qui se passe sur la box: <description>"** → tu flashes toi-même, tu me décris ce que HA voit, je debug si nécessaire
