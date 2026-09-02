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
 * Power meter library.
 */

#ifndef wb_power_h
#define wb_power_h

void power_init();

void power_reset_energy();

void power_do_reset_energy();

void power_update();

int power_read_energy();

/* Live session energy in Wh as a float (1-decimal resolution for MQTT).
   Reads the HLW8012 counter directly; does NOT touch persisted config. */
float power_read_live_session_energy_float();

/* Force an immediate persistence of the meter.* counters, bypassing the
   tick throttle. Call at charge-stop and before a planned reboot to avoid
   losing up to the throttle window of accumulated energy on power loss. */
void power_flush();

int power_read_active_power();

unsigned int power_read_voltage();

double power_read_current();

#endif /* wb_power_h */
