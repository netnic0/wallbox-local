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
  int energy = (int) power_read_energy() / 3600;
  int previousEnergy = mgos_sys_config_get_ocpp_transaction_consumption();
  int resetEnergy = mgos_sys_config_get_ocpp_transaction_reset_consumption();
  int previousUptime = mgos_sys_config_get_ocpp_transaction_uptime();
  int uptime = mgos_uptime();
  int intensity = 0;

  if (energy == previousEnergy) {
    return;
  }

  if (energy <= 0) {
    LOG(LL_ERROR, ("Negative energy %d", energy));
    return;
  }

  if (uptime <= previousUptime) {
    LOG(LL_ERROR, ("Invalid uptime %d, less than previous value %d", uptime, previousUptime));
  } else {
    double coeff = 3600 / (uptime - previousUptime);
    intensity = (energy - previousEnergy) * coeff / 240;
    mgos_sys_config_set_ocpp_transaction_intensity(intensity);
    LOG(LL_DEBUG, ("Intensity: %dA / %ds", intensity, uptime - previousUptime));
  }

  mgos_sys_config_set_ocpp_transaction_uptime(uptime);
  mgos_sys_config_set_ocpp_transaction_consumption(resetEnergy + energy);
  mgos_sys_config_save(&mgos_sys_config, false, NULL);
}
