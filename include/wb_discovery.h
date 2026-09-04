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

/*
 * Home Assistant MQTT Discovery library.
 *
 * Publishes retained discovery configs under homeassistant/<component>/... so
 * Home Assistant auto-creates the wallbox entities. The publish is staged
 * (one topic per timer tick) and heap-guarded to stay safe on the ESP8266.
 */

#ifndef wb_discovery_h
#define wb_discovery_h

/* Register internal state. Call once at app init, after mqtt_init(). */
void discovery_init();

/* Kick off (or restart) the staged discovery publish. Safe to call on every
   MQTT (re)connect: it (re)arms the one-shot start timer. Honours the
   mqtt.ha_discovery config flag. */
void discovery_kick();

#endif /* wb_discovery_h */
