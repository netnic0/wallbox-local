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
 * OCPP library.
 */

#ifndef wb_ocpp_h
#define wb_ocpp_h

bool ocpp_is_connected();
void ocpp_connect_backend();

void ocpp_synchronize();

void ocpp_send_ocpp_heartbeat();
void ocpp_send_boot_notification();
void ocpp_send_ocpp_status_notification(const char *);
void ocpp_send_ocpp_meter_values();

void ocpp_send_ocpp_request(struct mg_connection *, const char *, const char *, struct mg_str);
void ocpp_send_ocpp_response(struct mg_connection *, const char *, const char *);

mg_str ocpp_change_configuration(const char *);
void ocpp_get_configuration(const char *, char *);
mg_str ocpp_reset_hard();
mg_str ocpp_reset_soft();
mg_str ocpp_reset(const char *);
mg_str ocpp_start_transaction(const char *);
mg_str ocpp_stop_transaction(const char *, const char *);
mg_str ocpp_stop_transaction(const char *);
mg_str ocpp_update_firmware(const char *);
void ocpp_update_transaction();

void ocpp_handle_ocpp_response(struct mg_connection *, const char *, const char *);
void ocpp_handle_ocpp_cmd(struct mg_connection *, const char *, const char *, const char *);

#endif /* wb_ocpp_h */
