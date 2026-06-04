# Wallbox-Local — Plan d'implémentation

> **Status au 2026-06-04 11:30** : Phase 0bis ✅ TERMINÉE. Plan détaillé Livraison 1 rédigé et reviewé par senior-plan-reviewer (APPROUVÉ AVEC 5 AMENDEMENTS, confidence 89%). En attente d'approbation user pour démarrer l'implémentation L1. **Nouvelle décision à acter : push du repo sur github.com/netnic0** (au lieu de l'origin actuel `sebastien-savalle`). Pas encore de remote configuré pour netnic0.

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

## 📍 État au 2026-06-04 11:30 — PAUSE EN COURS

### ✅ Phase 0bis terminée
- Docker 29.5.3 installé dans WSL Ubuntu 26.04
- Image `mgos/esp8266-build:2.2.1-1.5.0-r5` pullée (1.74 GB) — note: tag `2.17.0-1.5.0-r5` du PLAN.md initial **N'EXISTE PAS** sur Docker Hub
- Image `mgos/mos:latest` pullée (orchestre le build via Docker socket)
- Serveurs Mongoose OS officiels DOWN (`mongoose-os.com/downloads → 404`) — `mos build --remote` mort, seul `--local` fonctionne
- `npm install` (629 packages) + `npm run webpack` OK → `dist/index.html.gz` (18 KB)
- 1er build de validation **RÉUSSI** : `build/fw.zip` 935 950 bytes vs `fw (7).zip` 941 958 bytes (Δ −0.6%, structure identique)

### 📜 Commande de build de référence (à mémoriser)
```bash
cd /mnt/c/Users/I058304/Downloads/shelly-ocpp-wallbox
npm run webpack  # si pas déjà fait
docker run --rm \
  --entrypoint /bin/sh \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v $PWD:$PWD -w $PWD \
  mgos/mos:latest \
  -c 'git config --global --add safe.directory "*" && mos build --local --platform esp8266 --verbose'
# Sortie: build/fw.zip
```

### ✅ Plan Livraison 1 rédigé et reviewé
- Plan détaillé : `docs/PLAN-L1.md` (refactor backend, suppression OCPP 844 LoC, ajout RPCs SetRelay/ResetEnergy, MQTT enrichi power/voltage/current)
- Approche retenue : **A — Nettoyage chirurgical minimal** (9 étapes auto-vérifiables)
- Senior-plan-reviewer verdict : **APPROUVÉ AVEC 5 AMENDEMENTS** (confidence 89%)

### 🔴 5 Amendements obligatoires (à intégrer avant tout edit)
1. **`power_update()` doit migrer dans `process_loop()`** (sinon énergie figée) — il est actuellement appelé via `ocpp_synchronize() → power_update()` (`wb_ocpp.cpp:248`)
2. **`ocpp_reset_hard()` est utilisé dans 2 RPCs non-OCPP** (`wb_rpc.cpp:102, 117`) — remplacer par `mgos_system_restart_after(10000)` AVANT de supprimer wb_ocpp.cpp
3. **NE PAS retirer la lib `mongoose` de `mos.yml` en L1** — `rpc-ws` en dépend. Ne retirer que `ota-http-client`
4. **Stratégie migration énergie** : reco `(b)` repartir de 0 (HA recorder garde l'historique), sinon `(c)` flag `meter.migrated:bool` plutôt qu'heuristique sur valeur zéro
5. **Critère "grep ocpp = 0"** doit être reformulé : exception si migration énergie au boot conservée

### 🎯 Décisions implicitement validées par la review
- Q2 `connected` → **(c)** toujours `true` (sentinelle de présence pour `expire_after: 180` HA)
- Q3 `tid` → **(a)** garder `tid: 0` en dur (rétro-compat HA)
- Q4 spike HLW8012 → **invalide** : `mgos_hlw8012_readVoltage()` (returns `unsigned int`) et `readCurrent()` (returns `double`) confirmés présents dans `deps/mongoose-lib-hlw8012/include/mgos_hlw8012.h`

### 🐙 Demande user en cours : push sur github.com/netnic0
**État** :
- Origin actuel : `https://github.com/sebastien-savalle/shelly-ocpp-wallbox.git` (fetch + push)
- Branch : `master` à HEAD `58c3691`
- Files non commités :
  - `PLAN.md` (untracked — plan haut niveau)
  - `docs/PLAN-L1.md` (untracked — plan détaillé Livraison 1)
- `gh` CLI **non installé dans WSL Ubuntu** (seulement git)
- Aucun remote `netnic0` configuré

### 📌 À FAIRE à la reprise (par ordre)

**Bloc A — Setup GitHub netnic0** (priorité immédiate selon dernière demande user)
1. Installer `gh` CLI dans WSL Ubuntu : `sudo apt install gh` (ou via le repo officiel cli.github.com)
2. Auth : `gh auth login` (ou utiliser un PAT existant via `gh auth login --with-token`)
3. Créer le repo `netnic0/wallbox-local` (nom à confirmer avec user — pas forcément `shelly-ocpp-wallbox` puisque le projet pivote)
4. Décider la stratégie :
   - **(a)** Nouveau repo vierge (squash de tout l'historique upstream + commit initial "fork: Wallbox-Local from sebastien-savalle/shelly-ocpp-wallbox @ 58c3691")
   - **(b)** Push complet de l'historique upstream + 1 commit "docs: add PLAN.md and PLAN-L1.md"
   - **(c)** Garder origin sebastien-savalle, ajouter remote netnic0 en parallèle
5. Configurer git user.email/user.name pour les commits (à confirmer : email perso ou pro ?)
6. Premier commit + push sur netnic0/master ou main
7. Couper l'accès origin sebastien-savalle si plus utile (`git remote remove origin`)

**Bloc B — Reprise Livraison 1** (après Bloc A)
1. Présenter au user les 5 amendements + 3 décisions issus du review (déjà fait dans la dernière réponse, à lui de valider explicitement)
2. Mettre à jour `docs/PLAN-L1.md` avec les amendements intégrés
3. Implémenter étape par étape (build de validation entre chaque) :
   - Étape 0 : (skip — spike inutile)
   - Étape 1 : ajout namespace `meter.*` dans `mos.yml`
   - Étape 2 : `power_read_voltage()` + `power_read_current()` dans `wb_power.h/cpp`
   - Étape 3 : RPCs `Wallbox.SetRelay`, `Wallbox.ResetEnergy` dans `wb_rpc.h/cpp`
   - Étape 4 : étendre JSON `MQTT_STATE` avec `power`, `voltage`, `current`
   - Étape 5 : déplacer `power_update()` dans `process_loop()` de `main.cpp` (+ remplacer `ocpp_synchronize()`)
   - Étape 6 : (selon décision Q1) migration énergie au boot OU repartir de 0
   - Étape 7 : substituer `ocpp_*` → équivalents (incl. les 2 `ocpp_reset_hard()` dans `wb_rpc.cpp`)
   - Étape 8 : supprimer `wb_ocpp.cpp/h`, retirer `ota-http-client` de `mos.yml`, bumper `version: 1.0.0`
   - Étape 9 : build final + check `manifest.name == Wallbox-Shelly1PM`, `version == 1.0.0`

### 💾 Memory dispo pour reprise
- `~/.claude/projects/C--Users-I058304/memory/project_wallbox-local-firmware.md`
- Transcript session : `C:\Users\I058304\.claude\projects\C--Users-I058304\5b29ef1b-709a-410f-a23a-d4c374d7f71e.jsonl`

---

## Comment reprendre demain

Dis-moi simplement :
> **"Reprends le projet wallbox depuis PLAN.md"**

ou

> **"On continue la Phase 0bis du wallbox"**

Je relirai automatiquement ce fichier et le memory associé, et on enchaîne sur l'installation Docker/mos dans WSL.
