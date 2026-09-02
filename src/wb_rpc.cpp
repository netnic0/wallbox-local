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

#include "wb_rpc.h"
#include "wb_power.h"
#include "wb_safety.h"
#include "wb_thermistor.h"
#include "wb_util.h"

#include "mgos.h"
#include "mgos_rpc.h"
#include "mgos_mqtt.h"

const char *RPC_GETINFO =
    "{"
    "id: %Q,"
    "sn: %Q,"
    "app: %Q,"
    "version: %Q,"
    "fw_build: %Q,"
    "fw_ts: %Q,"
    "mac: %Q,"
    "ip: %Q,"
    "uptime: %d,"
    "temperature: %.1f,"
    "wifi_ssid: %Q,"
    "wifi_ssid1: %Q,"
    "energy: %d,"
    "intensity: %d,"
    "state: %B,"
    "mqtt_state: %B,"
    "mqtt_server: %Q,"
    "mqtt_connected: %B,"
    "power: %d,"
    "voltage: %d,"
    "current: %.2f,"
    "charging: %B,"
    "ev: %B"
    "}";

void rpc_init() {
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.GetInfo", "", rpc_wallbox_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reboot", "", rpc_wallbox_reboot_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reset", "", rpc_wallbox_reset_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.ResetWifi", "", rpc_wallbox_reset_wifi_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.SetRelay", "{on: %B}", rpc_wallbox_set_relay_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.ResetEnergy", "", rpc_wallbox_reset_energy_handler, NULL);
}

void rpc_wallbox_get_info_handler(struct mg_rpc_request_info *ri,
                                  void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args) {
  char sn[25], ip[25];
  get_chargepoint_serial_number(sn);
  get_chargepoint_ip_address(ip);

  mg_rpc_send_responsef(ri,
                        RPC_GETINFO,
                         mgos_gpio_read(mgos_sys_config_get_gpio_relay()),
                         power_ev_detected()
                        sn,
                        MGOS_APP,
                        mgos_sys_ro_vars_get_fw_version(),
                        mgos_sys_ro_vars_get_fw_id(),
                        mgos_sys_ro_vars_get_fw_timestamp(),
                        mgos_sys_ro_vars_get_mac_address(),
                        ip,
                        (int) mgos_uptime(),
                        thermistor_read_celsius(),
                        mgos_sys_config_get_wifi_sta_ssid(),
                        mgos_sys_config_get_wifi_sta1_ssid(),
                        mgos_sys_config_get_meter_session_energy(),
                        mgos_sys_config_get_meter_intensity(),
                        mgos_gpio_read(mgos_sys_config_get_gpio_relay()),
                        mgos_sys_config_get_mqtt_enable(),
                         mgos_sys_config_get_mqtt_server(),
                         mgos_mqtt_global_is_connected(),
                        (int) power_read_active_power(),
                        (int) power_read_voltage(),
                        power_read_current(),
                        mgos_gpio_read(mgos_sys_config_get_gpio_relay()));

  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_reboot_handler(struct mg_rpc_request_info *ri,
                                void *cb_arg,
                                struct mg_rpc_frame_info *fi,
                                struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reboot"));

  /* Disarm safety timer before reboot so it does not fire during the 10s window */
  safety_disarm();

  /* Schedule a reboot in 10s, leaving time for the response to be sent. */
  mgos_system_restart_after(10000);

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_reset_handler(struct mg_rpc_request_info *ri,
                               void *cb_arg,
                               struct mg_rpc_frame_info *fi,
                               struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reset to factory settings"));

  /* Reset config first, then schedule a reboot. */
  mgos_config_reset(MGOS_CONFIG_LEVEL_VENDOR_4);
  mgos_system_restart_after(10000);

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_reset_wifi_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg,
                                    struct mg_rpc_frame_info *fi,
                                    struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reset Wi-Fi configuration"));

  // Reset WiFi config
  mgos_sys_config_set_wifi_ap_enable(true);
  mgos_sys_config_set_wifi_sta_enable(false);
  mgos_sys_config_set_wifi_sta_ssid("");
  mgos_sys_config_set_wifi_sta_pass("");
  mgos_sys_config_set_wifi_sta1_enable(false);
  mgos_sys_config_set_wifi_sta1_ssid("");
  mgos_sys_config_set_wifi_sta1_pass("");
  mgos_sys_config_set_provision_max_state(0);
  mgos_sys_config_save(&mgos_sys_config, false, NULL);

  mgos_system_restart_after(5000);

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_set_relay_handler(struct mg_rpc_request_info *ri,
                                   void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  bool on = false;
  if (json_scanf(args.p, args.len, ri->args_fmt, &on) != 1) {
    mg_rpc_send_errorf(ri, 400, "missing 'on' boolean argument");
    (void) cb_arg;
    (void) fi;
    (void) args;
    return;
  }

  LOG(LL_INFO, ("RPC SetRelay: %s", on ? "ON" : "OFF"));
  mgos_gpio_write(mgos_sys_config_get_gpio_relay(), on ? 1 : 0);

  if (on) {
    safety_arm();
  } else {
    safety_disarm();
    /* Turning the relay OFF ends a charge session: persist the meter counters
       immediately so a subsequent power loss does not lose accumulated Wh (#4). */
    power_flush();
  }

  /* NOTE: relay state is not persisted across reboots; at boot the
     relay is forced OFF for safety (see mgos_app_init). */
  mg_rpc_send_responsef(ri, "{relay: %B}", on);
  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_reset_energy_handler(struct mg_rpc_request_info *ri,
                                      void *cb_arg,
                                      struct mg_rpc_frame_info *fi,
                                      struct mg_str args) {
  LOG(LL_INFO, ("RPC ResetEnergy: zeroing meter counters"));
  power_do_reset_energy();

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}
