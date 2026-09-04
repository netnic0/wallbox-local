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

#include "wb_thermistor.h"

#include <math.h>

#include "mgos.h"
#include "mgos_adc.h"

const int pin = 0;                                   // A0 on esp8266
const float downstreamResistance = 10000.0f;         // 10 kOhm
const float referenceResistance = 32000.0f;          // 32 kOhm
const float referenceTemperature = 25.0f + 273.15f;  // 25°C (298.15 K)
const float beta = 3350.0f;                          // Beta: 3350
const float adcMax = 1024.0f;                        // Resolution: 1024, 0-1023

void thermistor_init() {
  mgos_adc_enable(pin);
}

float calculate_kelvin() {
  const int sampleCount = 5;
  float adcValue = 0.0f;

  /* Burst-sample the ADC without sleeping. The previous version slept 5x100ms
     (500 ms) which blocked the Mongoose OS cooperative event loop; this matters
     because this path runs from the 1s safety tick, the MQTT publish, and the
     GetInfo RPC. Consecutive mgos_adc_read() calls take only microseconds and
     still average out short-term noise. */
  for (int i = 0; i < sampleCount; i++) {
    adcValue += mgos_adc_read(pin);
  }
  adcValue = adcValue / sampleCount;

  /* Guard against a 0 reading (disconnected/at-rail ADC): avoid a divide-by-zero
     that would yield Inf/NaN and, after the safety comparison, a spurious trip. */
  if (adcValue <= 0.0f) {
    LOG(LL_WARN, ("Thermistor: ADC read 0, returning reference temperature"));
    return referenceTemperature;
  }

  // Log interpolation
  const float rValue = downstreamResistance / (adcMax / adcValue - 1.0);
  return 1.0 / ((log10f(rValue / referenceResistance) / beta) + (1.0 / referenceTemperature));
}

float thermistor_read_celsius() {
  const float temperature = calculate_kelvin() - 273.15f;
  LOG(LL_DEBUG, ("Temperature: %.1f C", temperature));
  return temperature;
}
