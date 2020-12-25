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

  int energy = power_read_energy();
  LOG(LL_INFO, ("Energy: %d Ws, %d Wh", energy, energy / 3600));
  LOG(LL_INFO, ("Heap: %d / %d b", mgos_get_free_heap_size(), mgos_get_heap_size()));
  LOG(LL_INFO, ("Temp: %.1f C", thermistor_read_celsius()));

  // OCPP
  if (ocpp_is_connected()) {
    ocpp_send_ocpp_heartbeat();

    if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
      ocpp_send_ocpp_meter_values();
    }
  } else {
    LOG(LL_INFO, ("Reconnecting to OCPP Backend"));
    ocpp_connect_backend();
  }

  // MQTT
  if (mqtt_is_connected()) {
    mqtt_send_topics();
  }

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