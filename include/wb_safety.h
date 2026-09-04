/*
 * Copyright (c) 2020 SAP Labs France, d-shop Caen
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Safety module - over-current and over-temperature protection.
 *
 * safety_init()   - call once at boot (before rpc_init / mqtt_init).
 * safety_arm()    - call when relay turns ON (charge session starts).
 * safety_disarm() - call when relay turns OFF (any path: MQTT cmd, RPC,
 *                   MQTT disconnect, WiFi lost, reboot handler).
 *
 * Thresholds are compile-time constants (hardcoded per user decision 2026-09-02).
 * No config keys are added to avoid flash-write overhead and to keep the safety
 * path free of config-system dependencies.
 */

#ifndef wb_safety_h
#define wb_safety_h

void safety_init();
void safety_arm();
void safety_disarm();

/*
 * Returns true once a safety trip (over-temp / over-current) has fired.
 * Latched until the next reboot. Callers that close the relay (RPC SetRelay,
 * MQTT "start") MUST check this and refuse to energize when tripped.
 */
bool safety_is_tripped();

#endif /* wb_safety_h */
