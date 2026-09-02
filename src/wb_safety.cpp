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

#include "wb_safety.h"
#include "wb_power.h"
#include "wb_thermistor.h"
#include "wb_mqtt.h"

#include "mgos.h"

/* -----------------------------------------------------------------------
 * Thresholds (hardcoded - user decision 2026-09-02)
 * ----------------------------------------------------------------------- */
#define SAFETY_TEMP_MAX_C      80.0f   /* °C: trip relay OFF immediately    */
#define SAFETY_CURRENT_MAX_A   12.0    /* A: over-current threshold         */
#define SAFETY_CURRENT_TICKS   5       /* consecutive 1-s ticks before trip */
#define SAFETY_TIMER_INTERVAL  1000    /* ms: safety polling interval       */

/* -----------------------------------------------------------------------
 * Module state (all static = no external visibility)
 * ----------------------------------------------------------------------- */
static mgos_timer_id s_timer_id      = MGOS_INVALID_TIMER_ID;
static int           s_overcurrent_count = 0;  /* consecutive over-limit ticks */

/* -----------------------------------------------------------------------
 * Forward declaration
 * ----------------------------------------------------------------------- */
static void safety_trip_relay(const char *reason);

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/*
 * Turn the relay OFF, flush energy counters, publish the new state, and log.
 * Called from the safety timer callback only (main event loop, no re-entrancy).
 */
static void safety_trip_relay(const char *reason) {
  LOG(LL_WARN, ("SAFETY TRIP: %s - turning relay OFF", reason));

  /* Turn relay off */
  mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);

  /* Persist energy counters before any potential reboot */
  power_flush();

  /* Disarm the safety timer (we are inside it, but guard is safe) */
  safety_disarm();

  /* Publish updated state so HA sees relay=false immediately */
  mqtt_send_state_topic();
}

/*
 * 1-second safety polling callback.
 * Checks over-temperature (immediate trip) and over-current (N consecutive).
 * Fires ONLY while a charge session is active (timer armed by safety_arm).
 */
static void safety_tick_cb(void *arg) {
  (void) arg;

  /* --- Over-temperature (immediate, single sample) --- */
  float temp = thermistor_read_celsius();
  if (temp > SAFETY_TEMP_MAX_C) {
    char msg[64];
    snprintf(msg, sizeof(msg), "over-temp %.1f > %.0f C", (double)temp,
             (double)SAFETY_TEMP_MAX_C);
    safety_trip_relay(msg);
    /* Reboot after a short delay so the log message can be flushed */
    LOG(LL_WARN, ("Rebooting in 5 s after over-temperature trip"));
    mgos_system_restart_after(5000);
    return;
  }

  /* --- Over-current (N consecutive ticks) --- */
  double current = power_read_current();
  if (current > SAFETY_CURRENT_MAX_A) {
    s_overcurrent_count++;
    LOG(LL_WARN, ("Safety: over-current %.2f A (tick %d/%d)",
                  current, s_overcurrent_count, SAFETY_CURRENT_TICKS));
    if (s_overcurrent_count >= SAFETY_CURRENT_TICKS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "over-current %.2f > %.0f A for %d s",
               current, SAFETY_CURRENT_MAX_A, SAFETY_CURRENT_TICKS);
      safety_trip_relay(msg);
    }
  } else {
    /* Current back under threshold: reset hysteresis counter */
    if (s_overcurrent_count > 0) {
      LOG(LL_INFO, ("Safety: current back to %.2f A, reset over-current counter",
                    current));
      s_overcurrent_count = 0;
    }
  }
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void safety_init() {
  s_timer_id         = MGOS_INVALID_TIMER_ID;
  s_overcurrent_count = 0;
}

void safety_arm() {
  /* Idempotent: if already armed, clear first to avoid double timers */
  if (s_timer_id != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(s_timer_id);
    s_timer_id = MGOS_INVALID_TIMER_ID;
  }
  s_overcurrent_count = 0;
  s_timer_id = mgos_set_timer(SAFETY_TIMER_INTERVAL, MGOS_TIMER_REPEAT,
                               safety_tick_cb, NULL);
  LOG(LL_INFO, ("Safety: armed (timer id %lu)", (unsigned long) s_timer_id));
}

void safety_disarm() {
  if (s_timer_id != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(s_timer_id);
    s_timer_id = MGOS_INVALID_TIMER_ID;
    LOG(LL_INFO, ("Safety: disarmed"));
  }
  s_overcurrent_count = 0;
}
