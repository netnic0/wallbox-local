# Wallbox-Local (Shelly 1PM Gen1) — Guide de Configuration et Intégration

Ce document explique comment configurer le firmware Wallbox-Local via l’interface Web intégrée, à quoi servent les champs, et comment intégrer la wallbox dans Home Assistant (HA) avec MQTT.

Chemin du projet (Windows): C:\\Users\\I058304\\HomeAssistant\\shelly-ocpp-wallbox

—

## 1. Présentation

## 1-ter. Check-list express (post-flash)

- UI: ouvrir http://<ip-de-la-wallbox>/ (Digest admin / wallbox), consulter Informations
- Wi-Fi: saisir SSID/pass (2 réseaux possibles) et Save; si besoin, Administration -> Reset Wi-Fi
- MQTT: cocher Enable, renseigner Server (ex: 192.168.1.10:1883), User/Password si nécessaire, Save
- Informations: vérifier MQTT Server (lecture seule) et MQTT Connected (Yes en vert)
- HA: intégration MQTT active, discovery activée; vérifier que l’appareil et ses entités apparaissent
- Test: Start/Stop, ResetEnergy; OTA via Firmware -> Upload build/fw.zip
- Appareil: Shelly 1PM Gen1 (ESP8266 + BL0937/HLW8012)
- Firmware: Mongoose OS
- Nom d’application (OTA): `Wallbox-Shelly1PM` (inévitable pour l’OTA stock)
- Fonctionnement: local-only (pas d’OCPP), contrôle via UI (RPC), métriques via MQTT, auto-découverte HA
- Sécurité: HTTP Digest (realm `wallbox`), utilisateur `admin`; AUCUN mot de passe par défaut — voir §1-quater

—

## 1-quater. Identifiants Digest (sécurité) — définir le mot de passe au build

L'UI Web, `/rpc` et `/update` (OTA) sont protégés par HTTP Digest
(utilisateur **`admin`**, réalm **`wallbox`**). **Il n'existe aucun mot de passe
par défaut** : c'est **vous qui le choisissez au moment du build**. Le mot de passe
est figé dans le fichier `fs/rpc_auth.htdigest`, généré localement puis embarqué
dans l'image du firmware.

### D'où vient le mot de passe ?

Le script `scripts/gen_htdigest.mjs` calcule la ligne htdigest
`admin:wallbox:MD5(admin:wallbox:<mot-de-passe>)` et l'écrit dans
`fs/rpc_auth.htdigest`. **Le mot de passe qui protégera l'appareil est exactement
celui que vous fournissez à ce script.**

> **⚠️ Où lancer le build ? Depuis WSL, pas PowerShell.**
> Le projet se construit sous **WSL2 (Ubuntu) + Docker** : `node_modules` y est
> installé pour Linux et l'outil `mos` tourne dans un conteneur. Si vous lancez
> `npm run build:local` depuis **PowerShell/cmd** avec un `node_modules` installé
> sous WSL, vous obtiendrez `'eslint' n'est pas reconnu ...` (il manque les shims
> `.cmd` que npm génère uniquement lors d'un `npm install` fait sous Windows).
> Suivez `docs/BUILD-AND-FLASH.md` et lancez toutes les commandes dans WSL.

### Étape 1 — définir le mot de passe puis builder (dans WSL)

Ouvrir un terminal WSL (taper `wsl` depuis PowerShell, ou lancer « Ubuntu »), puis :

```bash
cd /mnt/c/Users/I058304/HomeAssistant/shelly-ocpp-wallbox
export WALLBOX_ADMIN_PASS="MonMotDePasseFort"
npm run build:local        # gen-htdigest -> web build -> mos build -> build/fw.zip
```

`build:local` appelle automatiquement `gen-htdigest` en premier. Résultat :
utilisateur `admin`, mot de passe `MonMotDePasseFort`.

> **Le build firmware passe par Docker** (`mgos/mos:latest`), pas besoin d'installer
> le CLI `mos` sur la machine. `npm run mos-build:local` exécute `mos build --local`
> dans le conteneur (voir `scripts/mos_build.mjs`). Prérequis : Docker accessible
> (via WSL sur Windows) et l'image en cache (`docker pull mgos/mos:latest`).

> Le disque Windows `C:` est monté sous `/mnt/c` dans WSL. Le chemin
> `C:\Users\I058304\HomeAssistant\shelly-ocpp-wallbox` devient
> `/mnt/c/Users/I058304/HomeAssistant/shelly-ocpp-wallbox`.

Pour valider uniquement l'UI (lint + webpack), sans l'étape firmware `mos` :

```bash
cd /mnt/c/Users/I058304/HomeAssistant/shelly-ocpp-wallbox
export WALLBOX_ADMIN_PASS="MonMotDePasseFort"
npm run gen-htdigest -- --force   # écrase l'ancien htdigest avec CE mot de passe
npm run web-build                 # lint + format:css + webpack -> dist/
```

### Générer le htdigest seul (sans builder)

```bash
# via la variable d'environnement (recommandé : le mot de passe n'apparaît pas dans l'historique shell)
WALLBOX_ADMIN_PASS="MonMotDePasseFort" npm run gen-htdigest

# ou en passant le mot de passe en argument
node scripts/gen_htdigest.mjs --user admin --pass MonMotDePasseFort
```

### Si vous oubliez de définir le mot de passe

`gen-htdigest` **échoue volontairement** (`No password provided...`) et stoppe le
build — c'est voulu, pour ne jamais livrer d'identifiant par défaut. Définissez
`WALLBOX_ADMIN_PASS` (ou `--pass`) puis relancez.

### Quel mot de passe s'applique selon l'installation ?

| Situation | Mot de passe demandé |
|---|---|
| **1ʳᵉ installation** par flash série (`mos --port ... flash`) | **Aucun** — le canal UART est ouvert par l'ACL (`rpc.acl`) |
| Accès UI Web / OTA **après** cette installation | Celui du build (`WALLBOX_ADMIN_PASS`) |
| OTA sur un appareil qui tourne **déjà** ce firmware | L'ancien mot de passe (pour autoriser `/update`), le nouveau prend effet après reboot |

### Changer le mot de passe plus tard

Régénérez le fichier avec `--force`, puis reflashez (OTA ou série) :

```bash
WALLBOX_ADMIN_PASS="NouveauMotDePasse" node scripts/gen_htdigest.mjs --force
npm run build:local
```

### Notes

- Le réalm doit rester `wallbox` (doit correspondre à `http.auth_domain` /
  `rpc.auth_domain` dans `mos.yml`).
- Sans `--force`, un `fs/rpc_auth.htdigest` déjà présent n'est pas écrasé (on ne
  perd pas un identifiant posé par un opérateur).
- Modèle fourni : `docs/rpc_auth.htdigest.example` (hors de `fs/` pour ne pas être
  embarqué dans l'image).
- Ne committez jamais `fs/rpc_auth.htdigest` (déjà dans `.gitignore`).

—

## 1bis. Aperçu (capture d'écran de l'UI)

Pour illustrer la page de configuration, vous pouvez ajouter une capture d'écran.

- Placez votre image à l'emplacement suivant (Windows):
  - C:\\Users\\I058304\\HomeAssistant\\shelly-ocpp-wallbox\\docs\\images\\wallbox-ui-overview.png
- Ce dépôt affichera l'image via le lien ci-dessous (relatif pour GitHub):

![Wallbox UI Overview](docs/images/wallbox-ui-overview.png)

—

## 2. Accéder à l’interface Web de la Wallbox

1) Connectez la wallbox à votre réseau Wi-Fi (ou en AP/hotspot la première fois)
2) Ouvrez votre navigateur sur:
   - `http://wallbox.local/` (mDNS), ou
   - `http://<ip-de-la-wallbox>/` (ex: `http://192.168.1.57/`)
3) Le navigateur demande une authentification HTTP Digest:
   - Utilisateur: `admin`
   - Mot de passe: `wallbox` (par défaut). Changez-le après installation.

—

## 3. Contrôles et champs de l’UI

L’UI est structurée en sections: Informations, Wi-Fi, MQTT, Firmware, Administration, Logs.

### 3.1 Informations (lecture seule + actions)

- Status: État du relais (Charging/Idle)
- Power: Puissance active instantanée (W)
- Voltage: Tension secteur (V)
- Current: Courant (A; 2 décimales)
- EV detected: Présence détectée du véhicule (hystérésis EV) — voir §7
- Energy delivered: Énergie de la session en cours (Wh)
- Intensity: Intensité calculée (A) — issue du delta d’énergie et du temps à 230 V
- Temperature: Température interne (°C)
- MQTT Server: Adresse du broker configuré (lecture seule; cf. section MQTT)
- MQTT Connected: `Yes/No` (état de connexion au broker)
- Device/Serial/IP/MAC/Uptime: Informations système

Actions:

- Start charge: Ferme le relais (démarre la charge) — `Wallbox.SetRelay {on:true}`
- Stop charge: Ouvre le relais (arrête la charge) — `Wallbox.SetRelay {on:false}`
  - À l’arrêt, les compteurs d’énergie sont persistés immédiatement (sécurité en cas de coupure)
- Reset energy: Remet à zéro les compteurs d’énergie — `Wallbox.ResetEnergy`

### 3.2 Wi-Fi (configuration)

Vous pouvez configurer jusqu’à deux réseaux (lieux différents):

- Network 1 / Network 2:
  - SSID: nom du réseau
  - Password: mot de passe du réseau (bouton “Show password” pour visibilité)
- Sauvegarde: bouton “Save” pour appliquer — la wallbox bascule automatiquement vers un réseau disponible

Remarques:

- En cas de perte du Wi-Fi (station disconnect), le timer de sécurité est désarmé automatiquement pour éviter des coupures intempestives.
- “Reset Wi-Fi” (section Administration) efface les stations et réactive le mode AP pour reprovisionnement.

### 3.3 MQTT (configuration)

Le broker MQTT est optionnel pour l’UI, mais nécessaire pour Home Assistant (données et auto-découverte):

- Enable: cochez pour activer la publication MQTT
- Server: adresse et port du broker (ex: `192.168.1.10:1883`, ou `mqtt://192.168.1.10:1883`)
- User: (optionnel) identifiant du compte sur le broker
- Password: (optionnel) mot de passe du compte
- Save: enregistre et applique la configuration

Remarques:

- Le Last Will & Testament (LWT) est configuré pour publier `offline` sur `wallbox/<id>/availability` en cas de déconnexion.
- La découverte HA est activée par défaut (`mqtt.ha_discovery=true`).

### 3.4 Firmware (OTA)

- Update: chargez le fichier `build/fw.zip` pour mettre à jour le firmware
- Authentification: l’endpoint `/update` requiert HTTP Digest (`admin`, `wallbox`) — utilisez un navigateur ou:
  ```bash
  curl -v --digest -u admin:<motdepasse> -F file=@build/fw.zip http://<ip-de-la-wallbox>/update
  ```
- Succès: le dispositif redémarre automatiquement

---

## 3.4-bis. Build d'un firmware pre-configure (build:fw)

Le script `scripts/build_fw.mjs` produit un bundle `.zip` flashable avec la configuration Wi-Fi /
MQTT / device **baked-in** dans la couche utilisateur `conf9` (la couche de plus haute priorite,
appliquee au boot). Le dispositif demarre directement provisionne: aucune saisie manuelle dans l'UI.

### Principe technique

La configuration est injectee en tant que **partition `conf9` separee** dans le manifeste du bundle
via `mos create-fw-bundle`; l'image SPIFFS (fs.bin) n'est PAS modifiee. Au boot, Mongoose OS applique
la couche `conf9` par-dessus les valeurs par defaut (`conf0`) et toutes les couches vendor.

**Mapping Wi-Fi verifie contre la lib mongoose-os-libs/wifi:**

| Flag             | Cle conf9               | Slot                         |
| ---------------- | ----------------------- | ---------------------------- |
| `--wifi-ssid`  | `wifi.sta.ssid/pass`  | Reseau 1                     |
| `--wifi-ssid2` | `wifi.sta1.ssid/pass` | Reseau 2                     |
| (jamais)         | `wifi.sta2`           | AP-fallback — NE PAS ecrire |

### Prerequis

- Node.js >= 18 (pour `node:util` parseArgs)
- Docker installe dans WSL2 (`docker run` accessible depuis WSL)
- Image MOS: `mgos/mos:latest` (telechargee automatiquement au premier run)

### Utilisation

```bash
node scripts/build_fw.mjs [options]
```

Options disponibles:

| Option                | Description                                     | Cle conf9      |
| --------------------- | ----------------------------------------------- | -------------- |
| `--wifi-ssid <s>`   | SSID du reseau 1                                | wifi.sta.ssid  |
| `--wifi-pass <s>`   | Mot de passe reseau 1                           | wifi.sta.pass  |
| `--wifi-ssid2 <s>`  | SSID du reseau 2                                | wifi.sta1.ssid |
| `--wifi-pass2 <s>`  | Mot de passe reseau 2                           | wifi.sta1.pass |
| `--mqtt-enable`     | Active MQTT                                     | mqtt.enable    |
| `--mqtt-server <s>` | URL du broker (ex:`mqtt://192.168.1.10:1883`) | mqtt.server    |
| `--mqtt-user <s>`   | Identifiant MQTT                                | mqtt.user      |
| `--mqtt-pass <s>`   | Mot de passe MQTT                               | mqtt.pass      |
| `--device-id <s>`   | Identifiant de l'appareil                       | device.id      |
| `--out <path>`      | Chemin de sortie (defaut:`build/fw_conf.zip`) |                |
| `--no-build`        | Reutilise`build/fw.zip` existant (skip build) |                |
| `--keep-conf`       | Conserve`build/conf9.json` apres le bundle    |                |
| `--dry-run`         | Affiche la config prevue sans rien produire     |                |
| `-h, --help`        | Affiche l'aide                                  |                |

Ou via npm:

```bash
npm run build:fw -- --wifi-ssid "MonReseau" --wifi-pass "monpass"      --mqtt-enable --mqtt-server "mqtt://192.168.1.10:1883"      --mqtt-user "ha" --mqtt-pass "hapass"      --device-id "wallbox-salon"
```

### Valeurs par defaut (config.local.json ou .env)

Pour eviter de retaper les flags a chaque build, creez un fichier `config.local.json` (ou `.env`)
a la racine du depot (les deux sont gitignores):

**config.local.json:**

```json
{
  "wifiSsid": "MonReseau",
  "wifiPass": "monpass",
  "wifiSsid2": "Hotspot",
  "wifiPass2": "hotpass",
  "mqttEnable": true,
  "mqttServer": "mqtt://192.168.1.10:1883",
  "mqttUser": "ha",
  "mqttPass": "hapass",
  "deviceId": "wallbox-salon"
}
```

**ou .env:**

```env
WIFI_SSID=MonReseau
WIFI_PASS=monpass
WIFI_SSID2=Hotspot
WIFI_PASS2=hotpass
MQTT_ENABLE=true
MQTT_SERVER=mqtt://192.168.1.10:1883
MQTT_USER=ha
MQTT_PASS=hapass
DEVICE_ID=wallbox-salon
```

Les flags CLI ont priorite sur les fichiers. L'ordre de priorite: CLI > .env > config.local.json.

### Tester sans builder (dry-run)

```bash
node scripts/build_fw.mjs --wifi-ssid "Test" --mqtt-enable --dry-run
```

Affiche la conf9 prevue (mots de passe masques) sans rien ecrire sur le disque.

### Flasher le bundle produit

```bash
curl --digest -u admin:<motdepasse>      -F file=@build/fw_conf.zip      http://<ip-de-la-wallbox>/update
```

Le dispositif redемarre et demarre directement avec la configuration.

### IMPORTANT — Reset et conf9

La couche `conf9` est **effacee par tout factory reset** (6 reboots consecutifs OU RPC `Wallbox.Reset`).
Apres un reset, le dispositif perd la configuration baked-in. La recuperation est de re-flasher un
bundle produit par ce script.

### Securite

- `build/fw_conf.zip` et `build/conf9.json` contiennent des **credentials en clair**.
- Ils sont couverts par `.gitignore` (`build/` est ignore) — ne pas les committer ni les partager.
- `build/conf9.json` est supprime automatiquement apres le bundling (sauf `--keep-conf`).
- `config.local.json` et `.env` sont gitignores — ne pas les committer.

### 3.5 Administration

- Reboot: redémarre l’appareil (désarme le timer de sécurité avant reboot)
- Reset Wi-Fi: efface la config Wi-Fi (réactive l’AP), redémarre
- Factory Reset: réinitialise la configuration (niveau vendor), redémarre

### 3.6 Logs

- Téléchargez les fichiers de logs via la section Logs (liens listés si disponibles)

—

## 4. Intégration avec Home Assistant (HA)

### 4.1 Prérequis

- Un broker MQTT (ex: Mosquitto)
  - HA Add-on “Mosquitto broker” ou un broker externe
  - Créez un utilisateur dédié (ex: `wallbox`) et un mot de passe
- Intégration MQTT dans HA (Paramètres -> Appareils & Services -> Ajouter intégration -> MQTT)
- Auto-découverte HA activée (par défaut dans HA)

### 4.2 Mise en service

1) Configurez MQTT dans l’UI de la Wallbox (section MQTT): Enable, Server, User/Password
2) Redémarrez la Wallbox si nécessaire (Administration -> Reboot)
3) Vérifiez que HA découvre automatiquement la Wallbox (nouvel appareil et entités) via Home Assistant Discovery

### 4.3 Entités HA attendues

Entités publiées via MQTT Discovery (toutes retained):

- Sensors:
  - `power` (W) — mesure instantanée
  - `voltage` (V)
  - `current` (A)
  - `energy` (Wh) — énergie de la session
  - `temperature` (°C)
  - `uptime` (s) — state_class: total_increasing
- Binary sensors:
  - `charging` — relais fermé **ET** VE détecté (hysteresis courant). Un relais
    fermé sans VE branché n’est PAS « charging » (voir §7)
  - `ev` — détection EV via hysteresis (voir §7)
- Switch:
  - `relay` — commande Start/Stop via MQTT (topic `wallbox/<id>/cmd`)
- Availability:
  - `wallbox/<id>/availability` (online/offline)

### 4.5 Carte & dashboard Lovelace (prêts à l’emploi)

Un guide complet est fourni dans [`docs/HA-LOVELACE-CARD.md`](docs/HA-LOVELACE-CARD.md) :

- **Carte moderne** (Mushroom + card-mod + stack-in-card) : en-tête d’état
  dynamique (Charging / EV plugged / Idle avec couleur + badge offline), chips
  live power/current/voltage, session + température, bouton Start/Stop.
- **Dashboard complet** : vue dédiée « Wallbox » avec la carte de contrôle **et**
  les graphes d’historique — soit en `history-graph` natif (zéro dépendance),
  soit en `apexcharts-card` (plus joli, via HACS) pour la puissance et l’énergie.
- Prérequis HACS, méthode pour retrouver vos `entity_id` (placeholder
  `wallbox_XXXX` à remplacer), variante sans stack-in-card, astuce kWh, et
  dépannage.

> Ces cartes sont de la pure config Lovelace : **aucun impact sur le firmware**
> ni sur la flash de l’appareil.

### 4.4 Topics MQTT principaux

- `wallbox/<id>/state` — JSON métriques live:
  ```json
  {
    "uptime": 12345,
    "connected": true,
    "charging": false,
    "energy": 12.3,
    "intensity": 8,
    "tid": 0,
    "temperature": 45.6,
    "power": 3680,
    "voltage": 230,
    "current": 16.00,
    "ev": true
  }
  ```
- `wallbox/<id>/cmd` — commandes JSON:
  ```json
  { "action": "start" }
  { "action": "stop" }
  { "action": "reset_energy" }
  ```
- `wallbox/<id>/availability` — retained: `online` / `offline`

—

## 5. Sécurité (logicielle)

- HTTP/WS/MQTT protégés par HTTP Digest (`admin`, mot de passe défini au build — voir §1-quater)
- UART/serial (port de service) reste ouvert (pas d’auth) — réservé aux usages locaux/atelier
- Recommandations:
  - Définissez un vrai mot de passe au build (aucun mot de passe par défaut)
  - N’exposez pas l’UI sur Internet
  - Utilisez des identifiants dédiés sur le broker MQTT

—

## 5bis. Sécurité électrique & limites du montage (À LIRE avant de brancher un VE)

> ⚠️ **Ce montage n’est PAS une borne de recharge Mode 3.** Le Shelly 1PM ne pilote
> qu’un **relais tout-ou-rien 230 V** ; il n’a **pas de Control Pilot (CP/PWM)**
> IEC 61851. Il ne dialogue pas avec le véhicule et ne module pas le courant. Son
> seul rôle ici est de **couper/rétablir le 230 V en amont**.

### Usage supporté
- **Mode 2 uniquement** : câble de recharge avec **boîtier de contrôle intégré (ICCB)**
  côté prise domestique. C’est le boîtier du câble — pas le Shelly — qui assure le
  pilote et la sécurité côté VE.
- **Courant limité** : configuration testée ~**1.6 kW (~7 A)**, très en dessous des
  ~16 A nominaux du Shelly 1PM → marge thermique confortable pour une charge longue.
- Un vrai **câble/borne Mode 3 (Type 2) ne fonctionnera pas** (le VE attend le signal
  pilote, absent ici).

### Obligations d’installation (sous votre responsabilité)
- **Disjoncteur dédié** + **différentiel type A 30 mA** en amont du circuit.
- **Bornier bien serré** (un contact desserré est la 1re cause d’échauffement, même à 7 A
  pendant plusieurs heures), **section de câble adaptée**, **pas de multiprise/rallonge**.
- Shelly **ventilé** (pas enfermé hermétiquement). Prise murale correcte (idéalement renforcée).
- En cas de doute : **faites appel à un électricien qualifié** (230 V continu plusieurs heures).

### Rôle des protections firmware (garde-fous, PAS des protections réglementaires)
- Coupure **surchauffe à 80 °C** (relais OFF + reboot), avec **latch** empêchant toute
  refermeture réseau tant que le défaut est verrouillé (jusqu’au redémarrage).
- Coupure **sur-courant à 12 A** après quelques secondes.
- Ces seuils sont un **filet logiciel** ; ils **ne remplacent pas** le disjoncteur/différentiel.

### Rappel flashage
- **Ne jamais connecter le port série ET le 230 V simultanément** sur un Shelly 1PM :
  le GND peut être sur la phase → destruction du PC/Shelly, risque d’électrocution.
  Faites l’OTA (réseau) sous tension, ou le flash série **secteur débranché**.

—

## 6. Firmware OTA & Reset

- OTA: voir §3.4; après upload de `fw.zip`, l’appareil redémarre
- Reset:
  - Reset Wi-Fi: efface SSID/pass, réactive AP
  - Factory Reset: réinitialise la configuration (niveau vendor)
  - Reboot: redémarre

—

## 7. Détection EV (hystérésis)

- `ev` est calculé à partir de la puissance active:
  - EV présent si `power >= 150 W` sur 3 ticks consécutifs (1 tick ~ 60 s)
  - EV absent si `power <= 80 W` sur 3 ticks consécutifs
- Hystérésis pour éviter les oscillations et faux positifs
- `ev` est publié dans `state` et exposé côté HA (binary_sensor `ev`), et aussi disponible dans `Wallbox.GetInfo`

—

## 8. Dépannage

- `/update` renvoie 401:
  - Utilisez HTTP Digest: `--digest -u admin:<password>`
  - Assurez-vous que le navigateur relance la requête avec auth (Digest natif)
- L’UI charge mais tous les champs sont vides:
  - Attendu si vous n’êtes pas authentifié (Digest)
- HA ne découvre pas la Wallbox:
  - Vérifiez la connexion au broker MQTT, `mqtt.enable` et `mqtt.server`
  - Dans l'UI, vérifiez "MQTT Server" et "MQTT Connected"
  - Vérifiez que HA Discovery est activé
  - Consultez les logs de HA et de la Wallbox
- `docker: command not found` en build:
  - Lancez les commandes dans WSL (`wsl -e bash -lc '...'`) comme décrit dans `docs/BUILD-AND-FLASH.md`

—

## 9. Références

- Script de vérification SPIFFS: `scripts/verify_fs.sh` (WSL)
- Guide build/flash: `docs/BUILD-AND-FLASH.md`
- Plan d’implémentation: `docs/PLAN.md`
- Documentation RPC: `doc/rpc.md`
