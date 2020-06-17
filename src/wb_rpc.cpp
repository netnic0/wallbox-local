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
#include "wb_ocpp.h"
#include "wb_util.h"

#include "mgos.h"
#include "mgos_rpc.h"

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
    "wifi_ssid: %Q,"
    "energy: %d,"
    "state: %B,"
    "ocpp_url: %Q,"
    "ocpp_name: %Q,"
    "ocpp_state: %B,"
    "mqtt_state: %B,"
    "mqtt_server: %Q,"
    "mqtt_user: %Q"
    "}";

void rpc_init() {
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.GetInfo", "", rpc_wallbox_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reboot", "", rpc_wallbox_reboot_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reset", "", rpc_wallbox_reset_handler, NULL);
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
                        mgos_sys_config_get_device_id(),
                        sn,
                        MGOS_APP,
                        mgos_sys_ro_vars_get_fw_version(),
                        mgos_sys_ro_vars_get_fw_id(),
                        mgos_sys_ro_vars_get_fw_timestamp(),
                        mgos_sys_ro_vars_get_mac_address(),
                        ip,
                        (int) mgos_uptime(),
                        mgos_sys_config_get_wifi_sta_ssid(),
                        mgos_sys_config_get_ocpp_transaction_consumption(),
                        mgos_gpio_read(mgos_sys_config_get_gpio_relay()),
                        mgos_sys_config_get_ocpp_url(),
                        mgos_sys_config_get_ocpp_name(),
                        ocpp_is_connected(),
                        mgos_sys_config_get_mqtt_enable(),
                        mgos_sys_config_get_mqtt_server(),
                        mgos_sys_config_get_mqtt_user());
  (void) cb_arg;
  (void) fi;
  (void) args;
}

void rpc_wallbox_reboot_handler(struct mg_rpc_request_info *ri,
                                void *cb_arg,
                                struct mg_rpc_frame_info *fi,
                                struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reboot"));

  // OCPP reset and reboot
  ocpp_reset_hard();

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

  // OCPP reset and reboot
  ocpp_reset_hard();

  // Reset config
  mgos_config_reset(MGOS_CONFIG_LEVEL_USER);

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}
