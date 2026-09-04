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

#include "wb_discovery.h"

#include "mgos.h"
#include "mgos_mqtt.h"
#include <string.h>

/* Delay after CONNACK before the first discovery publish: let the TLS/MQTT
   handshake memory settle before we start allocating publish buffers. */
#define DISCOVERY_START_DELAY_MS 2000
/* One discovery topic is published per tick to avoid a heap-fragmenting burst. */
#define DISCOVERY_TICK_MS 300
/* Abort the sequence if free heap drops below this before a publish. */
#define DISCOVERY_MIN_HEAP 12000
/* Number of discovery topics (must match the entities table below). */
#define DISCOVERY_COUNT 10

/* One HA discovery entity. state_field is the JSON key in the frozen state
   topic (wallbox/<id>/state); NULL for entities that do not read a value
   (availability, switch command). */
struct discovery_entity {
  const char *component;    /* sensor | binary_sensor | switch */
  const char *object_id;    /* unique object suffix, e.g. "power" */
  const char *name;         /* human-readable entity name */
  const char *device_class; /* HA device_class or NULL */
  const char *unit;         /* unit_of_measurement or NULL */
  const char *state_field;  /* value_json.<field> source or NULL */
  const char *state_class;  /* HA state_class for sensors or NULL */
};

/* Order is stable; index drives the staged publish. */
static const struct discovery_entity discovery_entities[DISCOVERY_COUNT] = {
    {"sensor", "power", "Power", "power", "W", "power", "measurement"},
    {"sensor", "voltage", "Voltage", "voltage", "V", "voltage", "measurement"},
    {"sensor", "current", "Current", "current", "A", "current", "measurement"},
    {"sensor", "energy", "Session energy", "energy", "Wh", "energy", "measurement"},
    {"sensor", "temperature", "Temperature", "temperature", "°C", "temperature", "measurement"},
    {"sensor", "uptime", "Uptime", "duration", "s", "uptime", "total_increasing"},
    {"binary_sensor", "charging", "Charging", "battery_charging", NULL, "charging", NULL},
    {"switch", "relay", "Charge", NULL, NULL, "charging", NULL},
    {"availability", "availability", "", NULL, NULL, NULL, NULL},
    {"binary_sensor", "ev", "EV", "plug", NULL, "ev", NULL},
};

static int discovery_index = -1; /* -1 = idle; 0..COUNT-1 = publishing */
static mgos_timer_id discovery_start_timer = MGOS_INVALID_TIMER_ID;
static mgos_timer_id discovery_tick_timer = MGOS_INVALID_TIMER_ID;


static const char *device_id() {
  return mgos_sys_config_get_device_id();
}

/* Publish the discovery config for one entity (index). Retained.
   Buffers live on the stack; sizes are modest for the ESP8266 task stack. */
static void discovery_publish_one(int index) {
  const struct discovery_entity *e = &discovery_entities[index];
  const char *id = device_id();

  char topic[128];
  char payload[512];

  if (strcmp(e->component, "availability") == 0) {
    snprintf(topic, sizeof(topic),
             "homeassistant/binary_sensor/%s/availability/config", id);
    snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"Availability\","
             "\"uniq_id\":\"%s_availability\","
             "\"stat_t\":\"wallbox/%s/availability\","
             "\"pl_on\":\"online\",\"pl_off\":\"offline\","
             "\"dev_cla\":\"connectivity\","
             "\"dev\":{\"ids\":[\"%s\"],\"name\":\"Wallbox %s\","
             "\"mf\":\"SAP Labs France\",\"mdl\":\"Shelly 1PM Wallbox\"}"
             "}",
             id, id, id, id);
  } else if (strcmp(e->component, "switch") == 0) {
    snprintf(topic, sizeof(topic),
             "homeassistant/switch/%s/relay/config", id);
    snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s\","
             "\"uniq_id\":\"%s_relay\","
             "\"cmd_t\":\"wallbox/%s/cmd\","
             "\"pl_on\":\"{\\\"action\\\":\\\"start\\\"}\","
             "\"pl_off\":\"{\\\"action\\\":\\\"stop\\\"}\","
             "\"stat_t\":\"wallbox/%s/state\","
             "\"val_tpl\":\"{{ value_json.%s }}\","
             "\"stat_on\":\"true\",\"stat_off\":\"false\","
             "\"avty_t\":\"wallbox/%s/availability\","
             "\"dev\":{\"ids\":[\"%s\"]}"
             "}",
             e->name, id, id, id, e->state_field, id, id);
  } else if (strcmp(e->component, "binary_sensor") == 0) {
    snprintf(topic, sizeof(topic),
             "homeassistant/binary_sensor/%s/%s/config", id, e->object_id);
    snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s\","
             "\"uniq_id\":\"%s_%s\","
             "\"stat_t\":\"wallbox/%s/state\","
             "\"val_tpl\":\"{{ value_json.%s }}\","
             "\"pl_on\":\"true\",\"pl_off\":\"false\","
             "\"dev_cla\":\"%s\","
             "\"avty_t\":\"wallbox/%s/availability\","
             "\"dev\":{\"ids\":[\"%s\"]}"
             "}",
             e->name, id, e->object_id, id, e->state_field,
             e->device_class, id, id);
  } else {
    /* sensor */
    snprintf(topic, sizeof(topic),
             "homeassistant/sensor/%s/%s/config", id, e->object_id);
    snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s\","
             "\"uniq_id\":\"%s_%s\","
             "\"stat_t\":\"wallbox/%s/state\","
             "\"val_tpl\":\"{{ value_json.%s }}\","
             "\"unit_of_meas\":\"%s\","
             "\"dev_cla\":\"%s\","
             "\"stat_cla\":\"%s\","
             "\"avty_t\":\"wallbox/%s/availability\","
             "\"dev\":{\"ids\":[\"%s\"]}"
             "}",
             e->name, id, e->object_id, id, e->state_field,
             e->unit, e->device_class, e->state_class, id, id);
  }

  mgos_mqtt_pub(topic, payload, strlen(payload), 0 /* qos */, true /* retain */);
  LOG(LL_INFO, ("HA discovery %d/%d: %s", index + 1, DISCOVERY_COUNT, topic));
}

static void discovery_stop_tick() {
  if (discovery_tick_timer != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(discovery_tick_timer);
    discovery_tick_timer = MGOS_INVALID_TIMER_ID;
  }
  discovery_index = -1;
}

/* Publish one topic per tick; abort if heap is too low or connection lost. */
static void discovery_tick_cb(void *arg) {
  (void) arg;

  if (!mgos_mqtt_global_is_connected()) {
    LOG(LL_WARN, ("HA discovery: MQTT disconnected, aborting"));
    discovery_stop_tick();
    return;
  }

  if (mgos_get_free_heap_size() < DISCOVERY_MIN_HEAP) {
    LOG(LL_ERROR, ("HA discovery: low heap (%u), aborting at %d/%d",
                   (unsigned) mgos_get_free_heap_size(),
                   discovery_index, DISCOVERY_COUNT));
    discovery_stop_tick();
    return;
  }

  if (discovery_index < 0 || discovery_index >= DISCOVERY_COUNT) {
    discovery_stop_tick();
    return;
  }

  discovery_publish_one(discovery_index);
  discovery_index++;

  if (discovery_index >= DISCOVERY_COUNT) {
    LOG(LL_INFO, ("HA discovery: complete (%d topics)", DISCOVERY_COUNT));
    discovery_stop_tick();
  }
}

/* One-shot: fired DISCOVERY_START_DELAY_MS after (re)connect. */
static void discovery_start_cb(void *arg) {
  (void) arg;
  discovery_start_timer = MGOS_INVALID_TIMER_ID;

  if (!mgos_sys_config_get_mqtt_ha_discovery()) {
    return;
  }
  if (!mgos_mqtt_global_is_connected()) {
    return;
  }

  /* Restart cleanly if a previous run was still pending. */
  discovery_stop_tick();
  discovery_index = 0;
  discovery_tick_timer =
      mgos_set_timer(DISCOVERY_TICK_MS, MGOS_TIMER_REPEAT, discovery_tick_cb, NULL);
  LOG(LL_INFO, ("HA discovery: starting staged publish"));
}

void discovery_kick() {
  if (!mgos_sys_config_get_mqtt_ha_discovery()) {
    return;
  }
  /* Re-arm the one-shot start timer (coalesce rapid reconnects). */
  if (discovery_start_timer != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(discovery_start_timer);
  }
  discovery_start_timer =
      mgos_set_timer(DISCOVERY_START_DELAY_MS, 0, discovery_start_cb, NULL);
}

void discovery_init() {
  discovery_index = -1;
  discovery_start_timer = MGOS_INVALID_TIMER_ID;
  discovery_tick_timer = MGOS_INVALID_TIMER_ID;
}
