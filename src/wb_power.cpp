/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wb_power.h"

#include <limits.h>

#include "mgos.h"
#include "mgos_hlw8012.h"

static struct HLW8012 *hlw8012 = NULL;

void power_init() {
  if ((hlw8012 = mgos_hlw8012_create()) == NULL) {
    LOG(LL_ERROR, ("Cannot initialize HLW8012"));
    return;
  }
  mgos_hlw8012_begin(hlw8012,
                     mgos_sys_config_get_gpio_cf(),
                     mgos_sys_config_get_gpio_cf1(),
                     mgos_sys_config_get_gpio_sel(),
                     LOW,
                     true,
                     2000);

  mgos_hlw8012_setResistors(hlw8012, 0.001, 5 * 470000, 1000);
  mgos_hlw8012_setCurrentMultiplier(hlw8012, 25.7400);
  mgos_hlw8012_setVoltageMultiplier(hlw8012, 313.4000);
  mgos_hlw8012_setPowerMultiplier(hlw8012, 3414.2900);
}

void power_reset_energy() {
  if (hlw8012 == NULL) return;
  mgos_hlw8012_resetEnergy(hlw8012);
}

/* Reset all energy counters (HLW8012 internal + persisted meter.*).
 * Called by Wallbox.ResetEnergy RPC and the MQTT cmd action "reset_energy".
 * Safe no-op if HLW8012 is not initialized (the persisted counters are
 * still zeroed for a clean user-visible reset). */
void power_do_reset_energy() {
  power_reset_energy();
  mgos_sys_config_set_meter_total_energy(0);
  mgos_sys_config_set_meter_session_energy(0);
  mgos_sys_config_set_meter_intensity(0);
  mgos_sys_config_set_meter_uptime((int) mgos_uptime());
  mgos_sys_config_save(&mgos_sys_config, false, NULL);
}

int power_read_energy() {
  if (hlw8012 == NULL) return 0;
  unsigned long raw = mgos_hlw8012_readEnergy(hlw8012);
  if (raw > (unsigned long) INT_MAX) {
    LOG(LL_WARN, ("Energy counter saturated, capping at INT_MAX"));
    return INT_MAX;
  }
  return (int) raw;
}

int power_read_active_power() {
  if (hlw8012 == NULL) return 0;
  return mgos_hlw8012_readActivePower(hlw8012);
}

unsigned int power_read_voltage() {
  if (hlw8012 == NULL) return 0;
  return mgos_hlw8012_readVoltage(hlw8012);
}

double power_read_current() {
  if (hlw8012 == NULL) return 0.0;
  return mgos_hlw8012_readCurrent(hlw8012);
}

void power_update() {
  if (hlw8012 == NULL) return;

  int energy = power_read_energy() / 3600;  /* Wh since last HLW8012 reset (= session) */
  int previous_session = mgos_sys_config_get_meter_session_energy();
  int previous_total = mgos_sys_config_get_meter_total_energy();
  int previous_uptime = mgos_sys_config_get_meter_uptime();
  int uptime = (int) mgos_uptime();

  if (energy == previous_session) {
    return;  /* nothing to update */
  }

  if (energy < 0) {
    LOG(LL_ERROR, ("Negative energy %d, ignoring tick", energy));
    return;
  }

  /* Total energy is monotonic across HLW8012 resets and reboots.
     Increment only by the positive delta (a drop means HLW8012 was reset). */
  int delta_energy = energy - previous_session;
  if (delta_energy < 0) {
    delta_energy = energy;  /* HLW8012 was reset; the new value IS the delta */
  }
  int new_total = previous_total + delta_energy;

  /* Intensity = delta energy (Wh) over delta time (s) at 230 V (Europe).
     Approximation, not a true RMS current measurement. */
  int intensity = 0;
  if (uptime > previous_uptime && delta_energy > 0) {
    int dt = uptime - previous_uptime;
    intensity = (delta_energy * 3600) / (dt * 230);
    LOG(LL_DEBUG, ("Intensity: %dA over %ds", intensity, dt));
  } else if (uptime <= previous_uptime) {
    LOG(LL_ERROR, ("Invalid uptime %d, less than previous %d", uptime, previous_uptime));
  }

  mgos_sys_config_set_meter_session_energy(energy);
  mgos_sys_config_set_meter_total_energy(new_total);
  mgos_sys_config_set_meter_intensity(intensity);
  mgos_sys_config_set_meter_uptime(uptime);
  mgos_sys_config_save(&mgos_sys_config, false, NULL);
}
