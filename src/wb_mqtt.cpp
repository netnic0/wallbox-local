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
#include "wb_discovery.h"
#include "wb_power.h"
#include "wb_safety.h"
#include "wb_thermistor.h"
#include "wb_util.h"

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_vfs.h"
#include "frozen.h"
#include <string.h>

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
    "energy: %.1f,"
    "intensity: %d,"
    "tid: %d,"
    "temperature: %.1f,"
    "power: %d,"
    "voltage: %d,"
    "current: %.2f"
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
char mqtt_cmd_topic[60];
char mqtt_availability_topic[55];

/*
 * Handler for the MQTT command topic wallbox/<id>/cmd.
 * Expected payload: {"action":"start"} | {"action":"stop"} | {"action":"reset_energy"}
 * Re-subscription on reconnect is handled automatically by the MQTT lib.
 */
static void mqtt_cmd_handler(struct mg_connection *nc, const char *topic,
                             int topic_len, const char *msg, int msg_len,
                             void *ud) {
  (void) nc;
  (void) topic;
  (void) topic_len;
  (void) ud;

  /* msg is NOT null-terminated; always pass msg_len to json_scanf. */
  char *action = NULL;
  if (json_scanf(msg, msg_len, "{action: %Q}", &action) != 1 || action == NULL) {
    LOG(LL_WARN, ("MQTT cmd: malformed payload [%.*s]", msg_len, msg));
    free(action);  /* free(NULL) is a defined no-op per C11 §7.22.3.3 */
    return;
  }

  LOG(LL_INFO, ("MQTT cmd: action=%s", action));

  if (strcmp(action, "start") == 0) {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 1);
    safety_arm();
    mqtt_send_state_topic();
  } else if (strcmp(action, "stop") == 0) {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);
    safety_disarm();
    /* Charge just ended: persist the session energy now so an unexpected
       power loss after unplugging does not drop the last accumulated Wh (#4). */
    power_flush();
    mqtt_send_state_topic();
  } else if (strcmp(action, "reset_energy") == 0) {
    power_do_reset_energy();
    mqtt_send_state_topic();
  } else {
    LOG(LL_WARN, ("MQTT cmd: unknown action '%s', dropping", action));
  }

  free(action);
}

/*
 * Global MQTT event handler. On CONNACK (broker accepted the connection) we
 * publish the retained "online" availability message. The matching "offline"
 * message is registered as the broker-side Last-Will (see mqtt_init) and
 * delivered automatically if the TCP connection drops ungracefully.
 */
static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *ev_data,
                            void *user_data) {
  (void) nc;
  (void) ev_data;
  (void) user_data;
  if (ev == MG_EV_MQTT_CONNACK) {
    mgos_mqtt_pub(mqtt_availability_topic, "online", 6, 0 /* qos */, true /* retain */);
    LOG(LL_INFO, ("MQTT availability: online"));
    /* (Re)publish Home Assistant discovery configs on every (re)connect.
       Staged and heap-guarded inside wb_discovery.cpp. */
    discovery_kick();
  } else if (ev == MG_EV_MQTT_DISCONNECT) {
    /* MQTT connection lost: disarm the safety timer so it does not run
       without a connected broker (we cannot publish the tripped state). */
    safety_disarm();
  }
}

void mqtt_init() {
  snprintf(mqtt_announce_topic, sizeof(mqtt_announce_topic), "wallbox/%s/announce", mgos_sys_config_get_device_id());
  snprintf(mqtt_state_topic, sizeof(mqtt_state_topic), "wallbox/%s/state", mgos_sys_config_get_device_id());
  snprintf(mqtt_system_topic, sizeof(mqtt_system_topic), "wallbox/%s/system", mgos_sys_config_get_device_id());
  snprintf(mqtt_cmd_topic, sizeof(mqtt_cmd_topic), "wallbox/%s/cmd", mgos_sys_config_get_device_id());
  snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), "wallbox/%s/availability", mgos_sys_config_get_device_id());

  /* Register the Last-Will "offline" (retained) BEFORE the MQTT connection is
   * established. This build has no mgos_mqtt_set_will(); the LWT is carried by
   * the built-in mqtt.will_* config keys. Set in-memory only (no config save):
   * mqtt_init runs at app init, before the mqtt lib opens the connection, and
   * we do not want to wear flash or persist a device-id-derived topic. */
  mgos_sys_config_set_mqtt_will_topic(mqtt_availability_topic);
  mgos_sys_config_set_mqtt_will_message("offline");
  mgos_sys_config_set_mqtt_will_retain(1);

  /* Publish "online" once the broker acknowledges the connection. */
  mgos_mqtt_add_global_handler(mqtt_ev_handler, NULL);

  /* Subscribe to the command topic. The MQTT lib stores the subscription
   * and automatically re-subscribes on every reconnect — calling this
   * once is sufficient. */
  mgos_mqtt_sub(mqtt_cmd_topic, mqtt_cmd_handler, NULL);

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
    float energy = power_read_live_session_energy_float();
    int intensity = mgos_sys_config_get_meter_intensity();
    bool charging = mgos_gpio_read(mgos_sys_config_get_gpio_relay());
    double temperature = thermistor_read_celsius();
    unsigned int power = power_read_active_power();
    unsigned int voltage = power_read_voltage();
    double current = power_read_current();

    mgos_mqtt_pubf(mqtt_state_topic,
                   0,
                   false,
                   MQTT_STATE,
                   (int) mgos_uptime(),
                   true,    /* connected: sentinel of presence (Q2=c) */
                   charging,
                   energy,
                   intensity,
                   0,       /* tid: hardcoded for HA backward-compat (Q3=a) */
                   temperature,
                   (int) power,    /* frozen does not officially support %u */
                   (int) voltage,  /* values fit comfortably in signed int */
                   current);
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
