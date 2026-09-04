# Home Assistant — Wallbox Lovelace card (Mushroom + card-mod)

A modern, compact card for the Shelly 1PM Wallbox, built on the popular
**Mushroom** and **card-mod** custom cards. It shows live power / current /
voltage / energy / temperature, the charging & EV state, connectivity, and a
Start/Stop control — with a colour that follows the charging state.

![n/a](.) <!-- add a screenshot here once configured -->

## 1. Prerequisites (install via HACS)

Install these from **HACS → Frontend** (search by name), then reload the browser:

- **Mushroom** (`Mushroom` by piitaya)
- **card-mod** (`card-mod` by thomasloven)
- **stack-in-card** (`stack-in-card` by custom-cards) — groups the sub-cards into
  one seamless rounded card (optional but recommended for the look)

If you are not on the latest HA, also make sure `custom:mushroom-*` cards load
(hard-refresh with Ctrl-F5 after install).

## 2. Find your entity ids

The firmware publishes entities via MQTT Discovery under a device named
`Wallbox <device_id>`. The entity ids depend on your device id.

- Go to **Settings → Devices & Services → Entities**, filter by `wallbox`.
- Note the exact ids. They look like:
  - `sensor.wallbox_XXXX_power`
  - `sensor.wallbox_XXXX_current`
  - `sensor.wallbox_XXXX_voltage`
  - `sensor.wallbox_XXXX_session_energy`
  - `sensor.wallbox_XXXX_temperature`
  - `binary_sensor.wallbox_XXXX_charging`
  - `binary_sensor.wallbox_XXXX_ev`
  - `binary_sensor.wallbox_XXXX_availability`
  - `switch.wallbox_XXXX_charge`

In the YAML below, **replace every `wallbox_XXXX` with your real id** (a quick
find/replace of `wallbox_XXXX` does the whole card).

## 3. The card (YAML)

Add a **Manual card** (Dashboard → Edit → + Add card → Manual) and paste:

```yaml
type: custom:stack-in-card
mode: vertical
card_mod:
  style: |
    ha-card {
      border-radius: 20px;
      overflow: hidden;
      box-shadow: 0 4px 16px rgba(0,0,0,0.15);
    }
cards:
  # --- Header: title + charge state chip ---------------------------------
  - type: custom:mushroom-title-card
    title: Wallbox
    subtitle: Shelly 1PM

  - type: custom:mushroom-template-card
    primary: >-
      {% if is_state('binary_sensor.wallbox_XXXX_charging','on') %}
        Charging
      {% elif is_state('binary_sensor.wallbox_XXXX_ev','on') %}
        EV plugged (idle)
      {% else %}
        Idle
      {% endif %}
    secondary: >-
      {{ states('sensor.wallbox_XXXX_power') | int(0) }} W ·
      {{ states('sensor.wallbox_XXXX_current') | float(0) | round(1) }} A
    icon: >-
      {% if is_state('binary_sensor.wallbox_XXXX_charging','on') %}
        mdi:ev-station
      {% elif is_state('binary_sensor.wallbox_XXXX_ev','on') %}
        mdi:ev-plug-type2
      {% else %}
        mdi:power-plug-off
      {% endif %}
    icon_color: >-
      {% if is_state('binary_sensor.wallbox_XXXX_charging','on') %}
        green
      {% elif is_state('binary_sensor.wallbox_XXXX_ev','on') %}
        amber
      {% else %}
        grey
      {% endif %}
    badge_icon: >-
      {% if is_state('binary_sensor.wallbox_XXXX_availability','off') %}
        mdi:lan-disconnect
      {% endif %}
    badge_color: red

  # --- Live metrics: 3 chips (power / current / voltage) -----------------
  - type: custom:mushroom-chips-card
    alignment: center
    chips:
      - type: entity
        entity: sensor.wallbox_XXXX_power
        icon: mdi:flash
        icon_color: blue
      - type: entity
        entity: sensor.wallbox_XXXX_current
        icon: mdi:current-ac
        icon_color: teal
      - type: entity
        entity: sensor.wallbox_XXXX_voltage
        icon: mdi:sine-wave
        icon_color: purple

  # --- Session energy + temperature (two side-by-side entities) ----------
  - type: horizontal-stack
    cards:
      - type: custom:mushroom-entity-card
        entity: sensor.wallbox_XXXX_session_energy
        name: Session
        icon: mdi:counter
        icon_color: indigo
      - type: custom:mushroom-entity-card
        entity: sensor.wallbox_XXXX_temperature
        name: Temp
        icon: mdi:thermometer
        icon_color: >-

## 4. Variant without `stack-in-card` (Mushroom + card-mod only)

If you prefer to install only Mushroom + card-mod, drop the outer
`custom:stack-in-card` wrapper and paste the inner `cards:` list one by one as
separate cards in a vertical layout. The look is nearly identical, only the
single rounded container is lost.

## 5. Nice-to-have: show energy in kWh

`session_energy` is published in **Wh**. If you prefer kWh, either set the
entity's unit in HA (Settings -> Entities -> the sensor -> unit `kWh`), or use a
template chip that divides by 1000 and rounds to 2 decimals.

## 6. Troubleshooting

- **"Custom element doesn't exist: mushroom-..."** -> the custom card is not
  installed/loaded. Re-check the HACS install and hard-refresh (Ctrl-F5).
- **Entities "unavailable"** -> the device is offline (check
  `binary_sensor.wallbox_XXXX_availability`) or MQTT/HA is down. The firmware
  publishes an LWT `offline` on disconnect.
- **Templates render as raw text** -> use `custom:mushroom-template-card` for
  templated primary/icon fields (plain cards do not evaluate Jinja).
- **The switch does not react** -> confirm the command topic works: publish
  `{"action":"start"}` / `{"action":"stop"}` to `wallbox/<id>/cmd`.

## Notes

- `charging` is **relay ON AND EV drawing current** (hysteresis). A closed relay
  with no EV plugged shows *EV plugged (idle)* / *Idle*, not *Charging* -- this is
  intentional (see firmware `wb_mqtt.cpp` / `wb_rpc.cpp`).
- All entity ids use the placeholder `wallbox_XXXX`; replace with your real device
  id (see section 2) with a single find/replace.
- This card is pure Lovelace config -- **no firmware impact**, nothing on the
  device's limited flash.

          {{ 'red' if states('sensor.wallbox_XXXX_temperature') | float(0) > 60
             else 'green' }}

  # --- Start / Stop control ---------------------------------------------
  - type: custom:mushroom-entity-card
    entity: switch.wallbox_XXXX_charge
    name: Charge
    icon: mdi:power
    tap_action:
      action: toggle
    card_mod:
      style: |
        ha-card {
          --card-primary-color: {% if is_state('switch.wallbox_XXXX_charge','on') %}
            var(--rgb-green) {% else %} var(--rgb-grey) {% endif %};
        }
```
