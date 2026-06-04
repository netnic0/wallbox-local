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

#include "mgos.h"

void generate_uuid(char *uuid) {
  int random = mgos_rand_range(0.0, 999.0);
  sprintf(uuid,
          "%08lx-%04lx-1%03lx-a%03lx-%s",
          (unsigned long) time(NULL),
          (unsigned long) mgos_uptime() & 0xFFFFUL,
          (unsigned long) mgos_uptime() & 0xFFFUL,
          (unsigned long) random,
          mgos_sys_ro_vars_get_mac_address());
}

void get_current_date(char *buffer) {
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // 2020-04-13T11:39:35.116Z
  strftime(buffer, 21, "%FT%TZ", timeinfo);

  (void) rawtime;
}

void get_chargepoint_serial_number(char *sn) {
  sprintf(sn, "534C46434652%s", mgos_sys_ro_vars_get_mac_address());
}

void get_chargepoint_ip_address(char *ip) {
  struct mgos_net_ip_info ip_info;
  memset(&ip_info, 0, sizeof(ip_info));
  if (mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, MGOS_NET_IF_WIFI_STA, &ip_info)) {
    mgos_net_ip_to_str(&ip_info.ip, ip);
  } else if (mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, MGOS_NET_IF_WIFI_AP, &ip_info)) {
    mgos_net_ip_to_str(&ip_info.ip, ip);
  }
  (void) ip_info;
}
