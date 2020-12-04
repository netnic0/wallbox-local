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
#include "wb_util.h"

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_vfs.h"

const char *MQTT_ANNOUNCE =
    "{"
    "id: %Q,"
    "app: %Q,"
    "version: %Q,"
    "sn: %Q,"
    "fw: %Q,"
    "mac: %Q,"
    "ip: %Q"
    "}";

const char *MQTT_STATE =
    "{"
    "uptime: %d,"
    "connected: %B,"
    "charging: %B,"
    "energy: %d,"
    "tid: %d"
    "}";

const char *MQTT_SYSTEM =
    "{"
    "heapSize: %d,"
    "freeHeapSize: %d,"
    "minFreeHeapSize: %d,"
    "fsSize: %d,"
    "fsFreeSpace: %d"
    "}";

bool mqtt_announced = false;
char mqtt_announce_topic[50];
char mqtt_state_topic[50];
char mqtt_system_topic[50];

void mqtt_init() {
  sprintf(mqtt_announce_topic, "wallbox/%s/announce", mgos_sys_config_get_device_id());
  sprintf(mqtt_state_topic, "wallbox/%s/state", mgos_sys_config_get_device_id());
  sprintf(mqtt_system_topic, "wallbox/%s/system", mgos_sys_config_get_device_id());

  mqtt_send_topics();
}

void mqtt_reset() {
  mqtt_announced = false;
}

bool mqtt_is_connected() {
  return mgos_mqtt_global_is_connected();
}

void mqtt_send_topics() {
  if (mgos_mqtt_global_is_connected()) {
    if (!mqtt_announced) {
      mqtt_send_announce_topic();
    }
    mqtt_send_state_topic();
    mqtt_send_system_topic();
  }
}

void mqtt_send_announce_topic() {
  if (mgos_mqtt_global_is_connected()) {
    char sn[25], ip[25];
    get_chargepoint_serial_number(sn);
    get_chargepoint_ip_address(ip);

    mgos_mqtt_pubf(mqtt_announce_topic,
                   0,
                   true,
                   MQTT_ANNOUNCE,
                   mgos_sys_config_get_device_id(),
                   MGOS_APP,
                   mgos_sys_ro_vars_get_fw_version(),
                   sn,
                   mgos_sys_ro_vars_get_fw_id(),
                   mgos_sys_ro_vars_get_mac_address(),
                   ip);
    mqtt_announced = true;
  }
}

void mqtt_send_state_topic() {
  if (mgos_mqtt_global_is_connected()) {
    int energy = mgos_sys_config_get_ocpp_transaction_consumption();
    bool charging = mgos_gpio_read(mgos_sys_config_get_gpio_relay());
    int tid = mgos_sys_config_get_ocpp_transaction_id();
    if (tid < 0) {
      tid = 0;
    }
    mgos_mqtt_pubf(
        mqtt_state_topic, 0, false, MQTT_STATE, (int) mgos_uptime(), ocpp_is_connected(), charging, energy, tid);
  }
}

void mqtt_send_system_topic() {
  if (mgos_mqtt_global_is_connected()) {
    mgos_mqtt_pubf(mqtt_system_topic,
                   0,
                   false,
                   MQTT_SYSTEM,
                   mgos_get_heap_size(),
                   mgos_get_free_heap_size(),
                   mgos_get_min_free_heap_size(),
                   mgos_vfs_get_space_total("/"),
                   mgos_vfs_get_space_free("/"));
  }
}
