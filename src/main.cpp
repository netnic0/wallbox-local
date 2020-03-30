/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
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

#include "mgos.h"
#include "mgos_app.h"
#include "mgos_rpc.h"
#include "mgos_hlw8012.h"
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

static struct HLW8012 *hlw8012 = NULL;

static void timer_cb(void *arg) {
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
#ifdef LED_PIN
  mgos_gpio_toggle(LED_PIN);
#endif
  (void) arg;
}

static void shelly_get_info_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg, struct mg_rpc_frame_info *fi,
                                    struct mg_str args) {
  const char *ssid = mgos_sys_config_get_wifi_sta_ssid();
  const char *pass = mgos_sys_config_get_wifi_sta_pass();
  mg_rpc_send_responsef(
      ri,
      "{id: %Q, app: %Q, version: %Q, fw_build: %Q, current: %d, voltage: %d, energy: %d, "
      "sw1: {id: %d, name: %Q, in_mode: %d, persist: %B, state: %B},"
      "wifi_en: %B, wifi_ssid: %Q, wifi_pass: %Q} ",
      mgos_sys_config_get_device_id(), MGOS_APP,
      mgos_sys_ro_vars_get_fw_version(), mgos_sys_ro_vars_get_fw_id(),
      mgos_hlw8012_readCurrent(hlw8012), mgos_hlw8012_readVoltage(hlw8012), mgos_hlw8012_readEnergy(hlw8012),
      mgos_sys_config_get_sw1_id(), mgos_sys_config_get_sw1_name(),
      mgos_sys_config_get_sw1_in_mode(),
      mgos_sys_config_get_sw1_persist_state(), mgos_gpio_read(mgos_sys_config_get_sw1_in_gpio()),
      mgos_sys_config_get_wifi_sta_enable(), (ssid ? ssid : ""),
      (pass ? pass : ""));
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void shelly_set_switch_handler(struct mg_rpc_request_info *ri,
                                      void *cb_arg,
                                      struct mg_rpc_frame_info *fi,
                                      struct mg_str args) {
  int id = -1;
  bool state = false;

  json_scanf(args.p, args.len, ri->args_fmt, &id, &state);

  mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), state);
  mg_rpc_send_responsef(ri, NULL);

  (void) cb_arg;
  (void) fi;
}

static void shelly_get_conso_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg, struct mg_rpc_frame_info *fi,
                                    struct mg_str args) {
  mg_rpc_send_responsef(
      ri,
      "{current: %d, voltage: %d, energy: %d} ",
      mgos_hlw8012_readCurrent(hlw8012), mgos_hlw8012_readVoltage(hlw8012), mgos_hlw8012_readEnergy(hlw8012));
  (void) cb_arg;
  (void) fi;
  (void) args;
}

enum mgos_app_init_result mgos_app_init(void) {
#ifdef MGOS_HAVE_OTA_COMMON
  if (mgos_ota_is_first_boot()) {
    LOG(LL_INFO, ("Performing cleanup"));
    // In case we're uograding from stock fw, remove its files
    // with the exception of hwinfo_struct.json.
    remove("cert.pem");
    remove("passwd");
    remove("relaydata");
  }
#endif
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif
  if ((hlw8012 = mgos_hlw8012_create()) == NULL) {
    LOG(LL_INFO, ("Unable to initialize HLW8012"));
  }
  mgos_hlw8012_begin(hlw8012, mgos_sys_config_get_cf_pin(), mgos_sys_config_get_cf1_pin(), mgos_sys_config_get_sel_pin());

  mgos_set_timer(1000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);

  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetInfo", "", shelly_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetConso", "", shelly_get_conso_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.SetSwitch", "{id: %d, state: %B}", shelly_set_switch_handler, NULL);

  return MGOS_APP_INIT_SUCCESS;
}
