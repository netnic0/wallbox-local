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

#include "wb_mqtt.h"
#include "wb_ocpp.h"
#include "wb_power.h"
#include "wb_rpc.h"
#include "wb_thermistor.h"

#include "mgos.h"
#include "mgos_app.h"
#include "mgos_system.h"
#ifdef MGOS_HAVE_OTA_COMMON
#include "mgos_ota.h"
#endif

#ifndef MGOS_HAVE_WIFI
const char *mgos_sys_config_get_wifi_sta_ssid(void) {
  return "";
}
const char *mgos_sys_config_get_wifi_sta_pass(void) {
  return "";
}
bool mgos_sys_config_get_wifi_sta_enable(void) {
  return false;
}
#endif

#define MIN_HEAP_THRESHOLD 16 * 1024        // 16 kb
#define UPTIME_THRESHOLD 20 * 24 * 60 * 60  // 20 days in seconds

/*
 * Health check, return true if OK.
 */
bool healthcheck() {
  // Check for 16k limit
  if (mgos_get_free_heap_size() < MIN_HEAP_THRESHOLD) {
    LOG(LL_WARN, ("Health check: memory running low"));
    return false;
  }

  // Check for 20 days uptime
  if (mgos_uptime() > UPTIME_THRESHOLD) {
    LOG(LL_WARN, ("Health check: max uptime reached"));
    return false;
  }

  return true;
}

/*
 * Main loop.
 */
void process_loop(void *arg) {
  if (!healthcheck()) {
    // Reboot to avoid issues
    LOG(LL_INFO, ("Reboot caused by health check"));
    mgos_system_restart();
    return;
  }

  /* Power meter tick: aggregate energy from HLW8012 into meter.* namespace.
     Previously called via ocpp_synchronize() which is being removed. */
  power_update();

  // MQTT
  if (mqtt_is_connected()) {
    mqtt_send_topics();
  }

  (void) arg;
}

void reset_config(int level) {
  LOG(LL_INFO, ("Resetting config %d", level));
  mgos_config_reset(level);
  mgos_fs_gc();
  mgos_system_restart_after(100);
}

void set_reboot_counter(int value) {
  struct mgos_config *cfg = NULL;
  cfg = (struct mgos_config *) calloc(1, sizeof(*cfg));
  if (cfg != NULL) {
    if (mgos_sys_config_load_level(cfg, MGOS_CONFIG_LEVEL_VENDOR_4)) {
      mgos_config_set_reboot_counter(cfg, value);
      mgos_sys_config_set_reboot_counter(value);

      if (!mgos_sys_config_save_level(cfg, MGOS_CONFIG_LEVEL_VENDOR_4, false, NULL)) {
        LOG(LL_ERROR, ("Cannot save config (4)"));
      }
    } else {
      LOG(LL_ERROR, ("Cannot load config (4)"));
    }
  } else {
    LOG(LL_WARN, ("Cannot allocate space for config (4)"));
  }

  free(cfg);
}

void migrate_config() {
  struct mgos_config *cfg = NULL;
  cfg = (struct mgos_config *) calloc(1, sizeof(*cfg));
  if (cfg != NULL) {
    if (mgos_sys_config_load_level(cfg, MGOS_CONFIG_LEVEL_VENDOR_8)) {
      mgos_config_set_ocpp_url(cfg, mgos_sys_config_get_ocpp_url());
      mgos_config_set_ocpp_name(cfg, mgos_sys_config_get_ocpp_name());
      mgos_config_set_conf_version(cfg, 2);
      if (!mgos_sys_config_save_level(cfg, MGOS_CONFIG_LEVEL_VENDOR_8, false, NULL)) {
        LOG(LL_ERROR, ("Cannot save config (8)"));
      }
    } else {
      LOG(LL_ERROR, ("Cannot load config (8)"));
    }
  } else {
    LOG(LL_WARN, ("Cannot allocate space for config (8)"));
  }

  free(cfg);

  mgos_fs_gc();
  mgos_system_restart_after(100);
}

void reset_reboot_counter(void *arg) {
  set_reboot_counter(mgos_config_get_default_reboot_counter());
  (void) arg;
}

/*
 * Mongoose OS app init
 */
enum mgos_app_init_result mgos_app_init(void) {
#ifdef MGOS_HAVE_OTA_COMMON
  if (mgos_ota_is_first_boot()) {
    LOG(LL_INFO, ("Performing cleanup"));
    // In case we're upgrading from stock fw, remove its files
    // with the exception of hwinfo_struct.json.
    remove("cert.pem");
    remove("passwd");
    remove("relaydata");
  }
#endif

  LOG(LL_INFO, ("Starting Wallbox"));

  if (mgos_sys_config_get_conf_version() < 2) {
    migrate_config();
    return MGOS_APP_INIT_SUCCESS;
  }

  int rebootCounter = mgos_sys_config_get_reboot_counter() + 1;
  set_reboot_counter(rebootCounter);
  if (rebootCounter == 3) {
    reset_config(MGOS_CONFIG_LEVEL_USER);
    return MGOS_APP_INIT_SUCCESS;
  } else if (rebootCounter >= 6) {
    reset_config(MGOS_CONFIG_LEVEL_VENDOR_4);
    return MGOS_APP_INIT_SUCCESS;
  }
  mgos_set_timer(30000 /* ms */, 0, reset_reboot_counter, NULL);

  power_init();
  thermistor_init();

  mgos_set_timer(60000 /* ms */, MGOS_TIMER_REPEAT, process_loop, NULL);

  mgos_gpio_set_mode(mgos_sys_config_get_gpio_relay(), MGOS_GPIO_MODE_OUTPUT);

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 1);
  } else {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);
  }

  mgos_sys_config_set_ocpp_transaction_reset_consumption(mgos_sys_config_get_ocpp_transaction_consumption());
  mgos_sys_config_save(&mgos_sys_config, false, NULL);

  // RPC handlers
  rpc_init();

  // MQTT setup and announce
  mqtt_init();

  // OCPP
  ocpp_connect_backend();

  return MGOS_APP_INIT_SUCCESS;
}
