/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
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
#include "mgos_app.h"
#include "mgos_hlw8012.h"
#include "mgos_mqtt.h"
#include "mgos_ota_http_client.h"
#include "mgos_provision.h"
#include "mgos_rpc.h"
#include "mgos_system.h"
#include "mgos_vfs.h"
#include <limits.h>
#ifdef MGOS_HAVE_OTA_COMMON
#include "mgos_ota.h"
#endif

#define MAX_POWER 3680

#define OCPP_STATUS_AVAILABLE "Available"
#define OCPP_STATUS_CHARGING "Charging"
#define OCPP_STATUS_FINISHING "Finishing"
#define OCPP_STATUS_PREPARING "Preparing"

#define OCPP_RESET_TYPE_HARD "Hard"
#define OCPP_RESET_TYPE_SOFT "Soft"

#define OCPP_STOP_TRANSACTION_REASON_REMOTE "Remote"
#define OCPP_STOP_TRANSACTION_REASON_SOFTRESET "SoftReset"
#define OCPP_STOP_TRANSACTION_REASON_HARDRESET "HardReset"

#define OCPP_RESPONSE_ACCEPTED "{\"status\":\"Accepted\"}"
#define OCPP_RESPONSE_REJECTED "{\"status\":\"Rejected\"}"
#define OCPP_RESPONSE_NOTSUPPORTED "{\"status\":\"NotSupported\"}"

#define OCPP_REQUEST_BOOT_NOTIFICATION "BootNotification"
#define OCPP_REQUEST_GET_CONFIGURATION "GetConfiguration"
#define OCPP_REQUEST_CHANGE_CONFIGURATION "ChangeConfiguration"
#define OCPP_REQUEST_METER_VALUES "MeterValues"
#define OCPP_REQUEST_REMOTE_START_TRANSACTION "RemoteStartTransaction"
#define OCPP_REQUEST_REMOTE_STOP_TRANSACTION "RemoteStopTransaction"
#define OCPP_REQUEST_START_TRANSACTION "StartTransaction"
#define OCPP_REQUEST_STOP_TRANSACTION "StopTransaction"
#define OCPP_REQUEST_HEARTBEAT "Heartbeat"
#define OCPP_REQUEST_STATUS_NOTIFICATION "StatusNotification"
#define OCPP_REQUEST_RESET "Reset"
#define OCPP_REQUEST_UPDATE_FIRMWARE "UpdateFirmware"
#define OCPP_REQUEST_CLEAR_CACHE "ClearCache"
#define OCPP_REQUEST_UNLOCK_CONNECTOR "UnlockConnector"
#define OCPP_REQUEST_CHANGE_AVAILABILITY "ChangeAvailability"

const char *RPC_GETINFO =
    "{"
    "id: %Q,"
    "sn: %Q,"
    "app: %Q,"
    "version: %Q,"
    "fw_build: %Q,"
    "fw_ts: %Q,"
    "mac: %Q,"
    "ip: %Q,"
    "uptime: %d,"
    "wifi_ssid: %Q,"
    "energy: %d,"
    "power: %d,"
    "state: %B,"
    "ocpp_url: %Q,"
    "ocpp_name: %Q,"
    "ocpp_state: %B,"
    "mqtt_state: %B,"
    "mqtt_server: %Q,"
    "mqtt_user: %Q"
    "}";

const char *OCPP_BOOTNOTIFICATION =
    "{"
    "\"chargePointModel\":\"%s\","
    "\"chargePointSerialNumber\":\"%s\","
    "\"chargePointVendor\":\"SAP Labs France Caen\","
    "\"firmwareVersion\":\"%s (%s)\""
    "}";

const char *OCPP_STATUSNOTIFICATION =
    "{"
    "\"connectorId\":1,"
    "\"errorCode\":\"NoError\","
    "\"status\":\"%s\","
    "\"timestamp\":\"%s\""
    "}";

const char *OCPP_METERVALUES =
    "{"
    "\"connectorId\":1,"
    "\"transactionId\":%d,"
    "\"meterValue\":[{"
    "\"sampledValue\":[{"
    "\"unit\":\"Wh\","
    "\"context\":\"Sample.Periodic\","
    "\"value\":\"%d\""
    "}],"
    "\"timestamp\":\"%s\""
    "}]}";

const char *OCPP_STARTTRANSACTION =
    "{"
    "\"connectorId\":1,"
    "\"meterStart\":0,"
    "\"idTag\":\"%s\","
    "\"timestamp\":\"%s\""
    "}";

const char *OCPP_STOPTRANSACTION =
    "{"
    "\"meterStop\":%d,"
    "\"transactionId\":\"%d\","
    "\"idTag\":\"%s\","
    "\"timestamp\":\"%s\","
    "\"reason\":\"%s\""
    "}";

const char *OCPP_CONFIGURATION =
    "{\"configurationKey\":["
    "{\"key\":\"OCPPVersion\",\"readonly\":true,\"value\":\"1.6\"},"
    "{\"key\":\"OCPPCentralAddress\",\"readonly\":false,\"value\":\"%s\"},"
    "{\"key\":\"StationName\",\"readonly\":false,\"value\":\"%s\"},"
    "{\"key\":\"AuthorizationCacheEnabled\",\"readonly\":true,\"value\":false},"
    "{\"key\":\"AuthorizeRemoteTxRequests\",\"readonly\":true,\"value\":false},"
    "{\"key\":\"ClockAlignedDataInterval\",\"readonly\":true,\"value\":0},"
    "{\"key\":\"ConnectionTimeOut\",\"readonly\":true,\"value\":180},"
    "{\"key\":\"GetConfigurationMaxKeys\",\"readonly\":true,\"value\":32},"
    "{\"key\":\"HeartbeatInterval\",\"readonly\":false,\"value\":%d},"
    "{\"key\":\"LocalAuthorizeOffline\",\"readonly\":true,\"value\":false},"
    "{\"key\":\"LocalPreAuthorize\",\"readonly\":true,\"value\":false},"
    "{\"key\":\"MeterValuesAlignedData\",\"readonly\":true,\"value\":\"Energy.Active.Import.Register\"},"
    "{\"key\":\"MeterValuesSampledData\",\"readonly\":true,\"value\":\"Energy.Active.Import.Register\"},"
    "{\"key\":\"MeterValueSampleInterval\",\"readonly\":true,\"value\":60},"
    "{\"key\":\"NumberOfConnectors\",\"readonly\":true,\"value\":1},"
    "{\"key\":\"ResetRetries\",\"readonly\":true,\"value\":0},"
    "{\"key\":\"ConnectorPhaseRotation\",\"readonly\":true,\"value\":\"1.NotApplicable\"},"
    "{\"key\":\"ConnectorPhaseRotationMaxLength\",\"readonly\":true,\"value\":1},"
    "{\"key\":\"StopTransactionOnEVSideDisconnect\",\"readonly\":true,\"value\":true},"
    "{\"key\":\"StopTransactionOnInvalidId\",\"readonly\":true,\"value\":true},"
    "{\"key\":\"StopTxnAlignedData\",\"readonly\":true,\"value\":\"\"},"
    "{\"key\":\"StopTxnAlignedDataMaxLength\",\"readonly\":true,\"value\":0},"
    "{\"key\":\"StopTxnSampledData\",\"readonly\":true,\"value\":\"\"},"
    "{\"key\":\"StopTxnSampledDataMaxLength\",\"readonly\":true,\"value\":0},"
    "{\"key\":\"SupportedFeatureProfiles\",\"readonly\":true,\"value\":\"Core\"},"
    "{\"key\":\"TransactionMessageAttempts\",\"readonly\":true,\"value\":10},"
    "{\"key\":\"TransactionMessageRetryInterval\",\"readonly\":true,\"value\":60},"
    "{\"key\":\"UnlockConnectorOnEVSideDisconnect\",\"readonly\":true,\"value\":true}"
    "]}";

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
    "power: %d,"
    "connected: %B,"
    "charging: %B,"
    "energy: %d"
    "}";

const char *MQTT_SYSTEM =
    "{"
    "heapSize: %d,"
    "freeHeapSize: %d,"
    "minFreeHeapSize: %d,"
    "fsSize: %d,"
    "fsFreeSpace: %d"
    "}";

const char *WS_HEADER = "x-forwarded-for: %s\r\n";

#ifndef MGOS_HAVE_WIFI
const char *mgos_sys_config_get_wifi_sta_ssid(void) {
  return "";
}
const char *mgos_sys_config_get_wifi_sta_pass(void) {
  return "";
}
bool mgos_sys_config_get_wifi_sta_enable(void) {
  return false;
}
#endif

static struct HLW8012 *hlw8012 = NULL;
static bool ws_connected = false;
static char *tag_id = NULL;
static char start_transaction_uuid[50];
static char stop_transaction_uuid[50];
static char boot_notification_uuid[50];
static char default_uuid[50];
static struct mg_connection *ws_connection;
static time_t last_ocpp_interaction;
static bool mqtt_announced = false;
static char mqtt_announce_topic[50];
static char mqtt_state_topic[50];
static char mqtt_system_topic[50];

static void generate_uuid(char *uuid) {
  int random = mgos_rand_range(0.0, 999.0);
  sprintf(uuid,
          "%08lx-%04lx-1%03lx-a%03lx-%s",
          (unsigned long) time(NULL),
          (unsigned long) mgos_uptime() & 0xFFFFUL,
          (unsigned long) mgos_uptime() & 0xFFFUL,
          (unsigned long) random,
          mgos_sys_ro_vars_get_mac_address());
}

static void get_current_date(char *buffer) {
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // 2020-04-13T11:39:35.116Z
  strftime(buffer, 21, "%FT%TZ", timeinfo);
}

static void get_chargepoint_serial_number(char *sn) {
  sprintf(sn, "534C46434652%s", mgos_sys_ro_vars_get_mac_address());
}

static void get_chargepoint_ip_address(char *ip) {
  struct mgos_net_ip_info ip_info;
  memset(&ip_info, 0, sizeof(ip_info));
  if (mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, MGOS_NET_IF_WIFI_STA, &ip_info)) {
    mgos_net_ip_to_str(&ip_info.ip, ip);
  } else if (mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, MGOS_NET_IF_WIFI_AP, &ip_info)) {
    mgos_net_ip_to_str(&ip_info.ip, ip);
  }
  (void) ip_info;
}

static int compute_energy() {
  int energy = (int) mgos_hlw8012_readEnergy(hlw8012) / 3600;
  int previousEnergy = mgos_sys_config_get_ocpp_transaction_consumption();
  int resetEnergy = mgos_sys_config_get_ocpp_transaction_reset_consumption();

  if (energy == previousEnergy) {
    return previousEnergy;
  }

  if (energy <= 0) {
    LOG(LL_ERROR, ("Energy negative %d", energy));
    return previousEnergy;
  }
  if (energy > INT_MAX) {
    LOG(LL_ERROR, ("Energy %d exceeds INT_MAX %d", energy, INT_MAX));
    return previousEnergy;
  }

  energy = resetEnergy + energy;
  mgos_sys_config_set_ocpp_transaction_consumption(energy);
  mgos_sys_config_save(&mgos_sys_config, false, NULL);
  return energy;
}

static int compute_active_power() {
  int power = mgos_hlw8012_readActivePower(hlw8012);
  return power > MAX_POWER ? MAX_POWER : power;
}

static void wallbox_get_info_handler(struct mg_rpc_request_info *ri,
                                     void *cb_arg,
                                     struct mg_rpc_frame_info *fi,
                                     struct mg_str args) {
  char sn[25], ip[25];
  get_chargepoint_serial_number(sn);
  get_chargepoint_ip_address(ip);

  mg_rpc_send_responsef(ri,
                        RPC_GETINFO,
                        mgos_sys_config_get_device_id(),
                        sn,
                        MGOS_APP,
                        mgos_sys_ro_vars_get_fw_version(),
                        mgos_sys_ro_vars_get_fw_id(),
                        mgos_sys_ro_vars_get_fw_timestamp(),
                        mgos_sys_ro_vars_get_mac_address(),
                        ip,
                        (int) mgos_uptime(),
                        mgos_sys_config_get_wifi_sta_ssid(),
                        mgos_sys_config_get_ocpp_transaction_consumption(),
                        compute_active_power(),
                        mgos_gpio_read(mgos_sys_config_get_gpio_relay()),
                        mgos_sys_config_get_ocpp_url(),
                        mgos_sys_config_get_ocpp_name(),
                        ws_connected,
                        mgos_sys_config_get_mqtt_enable(),
                        mgos_sys_config_get_mqtt_server(),
                        mgos_sys_config_get_mqtt_user());
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void send_mqtt_announce() {
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

static void send_mqtt_state() {
  int power = compute_active_power();
  int energy = mgos_sys_config_get_ocpp_transaction_consumption();
  bool charging = mgos_gpio_read(mgos_sys_config_get_gpio_relay());
  mgos_mqtt_pubf(mqtt_state_topic, 0, false, MQTT_STATE, (int) mgos_uptime(), power, ws_connected, charging, energy);
}

static void send_mqtt_system() {
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

static void send_mqtt_topics() {
  if (!mqtt_announced) {
    send_mqtt_announce();
  }
  send_mqtt_state();
  send_mqtt_system();
}

static void send_ocpp_response(struct mg_connection *nc, const char *id, const char *data) {
  int length;
  char buf[2048], copy[2048];
  strcpy(copy, data);
  length = sprintf(buf, "[3, \"%s\", %s]", id, copy);
  LOG(LL_INFO, ("Sending response %.*s", length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);
  time(&last_ocpp_interaction);
}

static void send_ocpp_request(struct mg_connection *nc, const char *cmd, const char *id, struct mg_str data) {
  char buf[1024];
  int length;
  length = sprintf(buf, "[2, \"%s\", \"%s\", %s]", id, cmd, data.p);
  LOG(LL_DEBUG, ("Sending request %.*s", length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);
  time(&last_ocpp_interaction);
}

static void send_ocpp_heartbeat() {
  time_t now;
  time(&now);
  int interval = mgos_sys_config_get_ocpp_config_heartbeat_interval();
  double diff = difftime(now, last_ocpp_interaction);
  if (diff >= interval) {
    generate_uuid(default_uuid);
    send_ocpp_request(ws_connection, OCPP_REQUEST_HEARTBEAT, default_uuid, mg_mk_str("{}"));
  }
}

static void send_ocpp_status_notification(const char *status) {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);

  length = sprintf(buf, OCPP_STATUSNOTIFICATION, status, date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending status notification %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_STATUS_NOTIFICATION, default_uuid, content);
}

static void send_ocpp_meter_values() {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);
  int energy = compute_energy();

  length = sprintf(buf, OCPP_METERVALUES, mgos_sys_config_get_ocpp_transaction_id(), energy, date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending meter values %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_METER_VALUES, default_uuid, content);
}

static mg_str ocpp_stop_transaction(const char *reason) {
  LOG(LL_DEBUG,
      ("Stop transaction %d for tag %s, reason %s",
       mgos_sys_config_get_ocpp_transaction_id(),
       mgos_sys_config_get_ocpp_transaction_tag_id(),
       reason));

  send_ocpp_status_notification(OCPP_STATUS_FINISHING);
  mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);

  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(stop_transaction_uuid);

  int energy = compute_energy();

  length = sprintf(buf,
                   OCPP_STOPTRANSACTION,
                   energy,
                   mgos_sys_config_get_ocpp_transaction_id(),
                   mgos_sys_config_get_ocpp_transaction_tag_id(),
                   date_buffer,
                   reason);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending stop transaction %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_STOP_TRANSACTION, stop_transaction_uuid, content);

  if (mgos_mqtt_global_is_connected()) {
    send_mqtt_state();
  }

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

static mg_str ocpp_stop_transaction(const char *payload, const char *reason) {
  int id = 0;
  if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &id) > 0) {
    if (id == mgos_sys_config_get_ocpp_transaction_id()) {
      return ocpp_stop_transaction(reason);
    } else {
      LOG(LL_ERROR,
          ("Payload %s not matching current transaction id %d", payload, mgos_sys_config_get_ocpp_transaction_id()));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);
    }
  } else {
    LOG(LL_ERROR, ("Unable to find transaction id in payload %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

static mg_str ocpp_start_transaction(const char *payload) {
  if (json_scanf(payload, strlen(payload), "{ idTag:%Q }", &tag_id) > 0) {
    LOG(LL_INFO, ("Starting transaction for tag with id %s", tag_id));

    char buf[200];
    int length;

    char date_buffer[30];
    get_current_date(date_buffer);
    generate_uuid(start_transaction_uuid);

    send_ocpp_status_notification(OCPP_STATUS_PREPARING);

    length = sprintf(buf, OCPP_STARTTRANSACTION, tag_id, date_buffer);
    struct mg_str content = mg_mk_str_n(buf, length);
    LOG(LL_DEBUG, ("Sending start transaction %.*s", length, buf));
    send_ocpp_request(ws_connection, OCPP_REQUEST_START_TRANSACTION, start_transaction_uuid, content);

    if (mgos_mqtt_global_is_connected()) {
      send_mqtt_state();
    }

    return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
  } else {
    LOG(LL_WARN, ("Unable to find tag id in payload %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

static mg_str ocpp_reset_soft() {
  LOG(LL_INFO, ("Performing soft reset"));

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    ocpp_stop_transaction(OCPP_STOP_TRANSACTION_REASON_SOFTRESET);
  }

  mqtt_announced = false;

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

static mg_str ocpp_reset_hard() {
  LOG(LL_INFO, ("Performing hard reset"));

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    ocpp_stop_transaction(OCPP_STOP_TRANSACTION_REASON_HARDRESET);
  }

  mgos_system_restart_after(10000);

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

static mg_str ocpp_reset(const char *payload) {
  char *reset_type = NULL;

  if (json_scanf(payload, strlen(payload), "{ type:%Q }", &reset_type) > 0) {
    LOG(LL_INFO, ("Resetting in mode %s", reset_type));

    if (strcmp(OCPP_RESET_TYPE_SOFT, reset_type) == 0) {
      return ocpp_reset_soft();
    } else if (strcmp(OCPP_RESET_TYPE_HARD, reset_type) == 0) {
      return ocpp_reset_hard();
    } else {
      LOG(LL_WARN, ("Reset type %s not supported", reset_type));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);
    }
  }

  LOG(LL_WARN, ("Unable to find reset type in payload %s", payload));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

static mg_str ocpp_get_configuration(const char *payload) {
  char buf[1800];
  int length;
  LOG(LL_DEBUG, ("OCPP GetConfiguration request: %s", payload));
  length = sprintf(buf,
                   OCPP_CONFIGURATION,
                   mgos_sys_config_get_ocpp_url(),
                   mgos_sys_config_get_ocpp_name(),
                   mgos_sys_config_get_ocpp_config_heartbeat_interval());
  return mg_mk_str_n(buf, length);
}

static mg_str ocpp_change_configuration(const char *payload) {
  LOG(LL_DEBUG, ("OCPP ChangeConfiguration request: %s", payload));
  char *key = NULL;

  if (json_scanf(payload, strlen(payload), "{ key:%Q }", &key) > 0) {
    if (strcmp("HeartbeatInterval", key) == 0) {
      int value;
      if (json_scanf(payload, strlen(payload), "{ value:%d }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%d\"", key, value));
        if (value >= 60) {
          mgos_sys_config_set_ocpp_config_heartbeat_interval(value);
          mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
        LOG(LL_ERROR, ("ChangeConfiguration request with incorrect value for key: \"%s\", value \"%d\"", key, value));
        return mg_mk_str(OCPP_RESPONSE_REJECTED);
      }
      LOG(LL_ERROR, ("ChangeConfiguration request without number value for key: \"%s\"", key));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else if (strcmp("OCPPCentralAddress", key) == 0) {
      char *value = NULL;
      if (json_scanf(payload, strlen(payload), "{ value:%Q }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%s\"", key, value));
        if (value != NULL) {
          mgos_sys_config_set_ocpp_url(value);
          mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
      }
      LOG(LL_WARN, ("ChangeConfiguration request without value for key: \"%s\"", key));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else if (strcmp("StationName", key) == 0) {
      char *value = NULL;
      if (json_scanf(payload, strlen(payload), "{ value:%Q }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%s\"", key, value));
        if (value != NULL) {
          mgos_sys_config_set_ocpp_name(value);
          mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
      }
      LOG(LL_WARN, ("ChangeConfiguration request without value for key: \"%s\"", key));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else {
      LOG(LL_ERROR, ("ChangeConfiguration request for unsupported key: \"%s\"", key));
      return mg_mk_str(OCPP_RESPONSE_NOTSUPPORTED);
    }
  }
  LOG(LL_ERROR, ("No key for ChangeConfiguration request"));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

static mg_str ocpp_update_firmware(const char *payload) {
  char *location = NULL;

  if (json_scanf(payload, strlen(payload), "{ location:%Q }", &location) > 0) {
    LOG(LL_INFO, ("Updating firmware from %s", location));

    mgos_ota_http_start(location, NULL);
    return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
  }

  LOG(LL_WARN, ("Unable to find location in payload %s", payload));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

static void handle_ocpp_response(struct mg_connection *nc, const char *id, const char *payload) {
  LOG(LL_DEBUG, ("Handle ocpp response with id %s", id));
  if (strcmp(id, start_transaction_uuid) == 0) {
    int transaction_id;
    if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &transaction_id) > 0) {
      send_ocpp_status_notification(OCPP_STATUS_CHARGING);
      mgos_hlw8012_resetEnergy(hlw8012);
      mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 1);
      mgos_sys_config_set_ocpp_transaction_id(transaction_id);
      mgos_sys_config_set_ocpp_transaction_tag_id(tag_id);
      mgos_sys_config_set_ocpp_transaction_consumption(0);
      mgos_sys_config_set_ocpp_transaction_reset_consumption(0);
      mgos_sys_config_save(&mgos_sys_config, false, NULL);
      LOG(LL_INFO, ("Transaction started %d", transaction_id));
    } else {
      send_ocpp_status_notification(OCPP_STATUS_AVAILABLE);
      mgos_sys_config_set_ocpp_transaction_id(-1);
      mgos_sys_config_save(&mgos_sys_config, false, NULL);
      LOG(LL_ERROR, ("Failed to start transaction"));
    }
  } else if (strcmp(id, stop_transaction_uuid) == 0) {
    send_ocpp_status_notification(OCPP_STATUS_AVAILABLE);
    mgos_hlw8012_resetEnergy(hlw8012);
    mgos_sys_config_set_ocpp_transaction_consumption(0);
    mgos_sys_config_set_ocpp_transaction_reset_consumption(0);
    mgos_sys_config_set_ocpp_transaction_id(-1);
    mgos_sys_config_save(&mgos_sys_config, false, NULL);
  } else if (strcmp(id, boot_notification_uuid) == 0) {
    if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
      send_ocpp_status_notification(OCPP_STATUS_CHARGING);
    } else {
      send_ocpp_status_notification(OCPP_STATUS_AVAILABLE);
    }

    int value;
    if (json_scanf(payload, strlen(payload), "{ interval:%d }", &value) > 0) {
      int interval = mgos_sys_config_get_ocpp_config_heartbeat_interval();
      if (value > 0 && value != interval) {
        mgos_sys_config_set_ocpp_config_heartbeat_interval(value);
        mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
        LOG(LL_INFO, ("Heartbeat interval set to %d as per server request", value));
      }
    }
  }

  (void) nc;
}

static void handle_ocpp_cmd(struct mg_connection *nc, const char *cmd, const char *id, const char *payload) {
  LOG(LL_DEBUG, ("Handle ocpp cmd %s with id %s", cmd, id));
  struct mg_str data;
  if (strcmp(cmd, OCPP_REQUEST_GET_CONFIGURATION) == 0) {
    data = ocpp_get_configuration(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_CHANGE_CONFIGURATION) == 0) {
    data = ocpp_change_configuration(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_REMOTE_START_TRANSACTION) == 0) {
    data = ocpp_start_transaction(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_REMOTE_STOP_TRANSACTION) == 0) {
    data = ocpp_stop_transaction(payload, OCPP_STOP_TRANSACTION_REASON_REMOTE);
  } else if (strcmp(cmd, OCPP_REQUEST_RESET) == 0) {
    data = ocpp_reset(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_UPDATE_FIRMWARE) == 0) {
    data = ocpp_update_firmware(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_CLEAR_CACHE) == 0) {
    data = mg_mk_str(OCPP_RESPONSE_REJECTED);
  } else if (strcmp(cmd, OCPP_REQUEST_UNLOCK_CONNECTOR) == 0) {
    data = mg_mk_str(OCPP_RESPONSE_NOTSUPPORTED);
  } else if (strcmp(cmd, OCPP_REQUEST_CHANGE_AVAILABILITY) == 0) {
    data = mg_mk_str(OCPP_RESPONSE_REJECTED);
  } else {
    data = mg_mk_str("{}");
  }
  send_ocpp_response(nc, id, data.p);
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  switch (ev) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      LOG(LL_DEBUG, ("-- Connection status: %d", status));
      if (status != 0) {
        LOG(LL_ERROR, ("-- Connection error: %d", status));
      }
      break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
      LOG(LL_DEBUG, ("-- handshake Request"));
      break;
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      struct http_message *hm = (struct http_message *) ev_data;
      if (ws_connected == true) {
        LOG(LL_INFO, ("-- Already Connected"));
        break;
      }
      if (hm->resp_code == 101) {
        LOG(LL_INFO, ("-- Connected"));
        mgos_provision_set_cur_state(MGOS_PROVISION_ST_CLOUD_CONNECTED);
        ws_connected = true;

        char sn[25];
        get_chargepoint_serial_number(sn);
        char buf[1024];
        int length = sprintf(buf, OCPP_BOOTNOTIFICATION, mgos_sys_ro_vars_get_app(), sn, mgos_sys_ro_vars_get_fw_version(), mgos_sys_ro_vars_get_fw_timestamp());
        struct mg_str content = mg_mk_str_n(buf, length);
        generate_uuid(boot_notification_uuid);
        send_ocpp_request(nc, OCPP_REQUEST_BOOT_NOTIFICATION, boot_notification_uuid, content);
      } else {
        LOG(LL_ERROR, ("-- Connection failed! HTTP code %d", hm->resp_code));
        /* Connection will be closed after this. */
      }
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      LOG(LL_DEBUG, ("-- Frame %.*s", (int) wm->size, wm->data));
      if (wm->size < 2) {
        break;
      } else if (wm->data[1] == '2') {
        const char *msg = reinterpret_cast<const char *>(wm->data);
        char cmd[50];
        char uuid[50];
        char payload[500];
        struct json_token token;
        if (json_scanf_array_elem(msg, (int) wm->size, "", 1, &token) > 0) {
          sprintf(uuid, "%.*s", token.len, token.ptr);
        } else {
          break;
        }
        if (json_scanf_array_elem(msg, (int) wm->size, "", 2, &token) > 0) {
          sprintf(cmd, "%.*s", token.len, token.ptr);
        } else {
          break;
        }
        if (json_scanf_array_elem(msg, (int) wm->size, "", 3, &token) > 0) {
          sprintf(payload, "%.*s", token.len, token.ptr);
          handle_ocpp_cmd(nc, cmd, uuid, payload);
        } else {
          break;
        }
      } else if (wm->data[1] == '3') {
        const char *msg = reinterpret_cast<const char *>(wm->data);
        char uuid[50];
        char payload[500];
        struct json_token token;
        if (json_scanf_array_elem(msg, (int) wm->size, "", 1, &token) > 0) {
          sprintf(uuid, "%.*s", token.len, token.ptr);
        } else {
          break;
        }
        if (json_scanf_array_elem(msg, (int) wm->size, "", 2, &token) > 0) {
          sprintf(payload, "%.*s", token.len, token.ptr);
          handle_ocpp_response(nc, uuid, payload);
        } else {
          break;
        }
      }

      break;
    }
    case MG_EV_CLOSE: {
      LOG(LL_INFO, ("-- WS Disconnection"));
      ws_connected = false;
      break;
    }
  }
  (void) user_data;
}

static void connect_ocpp_backend() {
  if (mgos_sys_config_get_ocpp_url() != NULL && mgos_sys_config_get_ocpp_name() != NULL) {
    int urlLength = strlen(mgos_sys_config_get_ocpp_url());
    int nameLength = strlen(mgos_sys_config_get_ocpp_name());

    char buf[urlLength + nameLength + 2];
    int length = sprintf(
        buf, "%.*s/%.*s", urlLength, mgos_sys_config_get_ocpp_url(), nameLength, mgos_sys_config_get_ocpp_name());

    LOG(LL_INFO, ("Connecting to OCPP Backend %.*s", length, buf));

    char extraHeaders[128];
    char ip[25];
    get_chargepoint_ip_address(ip);
    sprintf(extraHeaders, WS_HEADER, ip);
    ws_connection = mg_connect_ws(mgos_get_mgr(), ev_handler, NULL, buf, "ocpp1.6", extraHeaders);
  } else {
    LOG(LL_WARN, ("OCPP Config is not defined !"));
  }
}

static void wallbox_reboot_handler(struct mg_rpc_request_info *ri,
                                   void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reboot"));

  // OCPP reset and reboot
  ocpp_reset_hard();

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void wallbox_reset_handler(struct mg_rpc_request_info *ri,
                                  void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args) {
  LOG(LL_INFO, ("RPC request to reset to factory settings"));

  // OCPP reset and reboot
  ocpp_reset_hard();

  // Reset config
  mgos_config_reset(MGOS_CONFIG_LEVEL_USER);

  mg_rpc_send_responsef(ri, "{}");
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void timer_cb(void *arg) {
  if (ws_connected == true) {
    send_ocpp_heartbeat();
    int energy = mgos_hlw8012_readEnergy(hlw8012);
    LOG(LL_INFO,
        ("Energy Ws %d, Energy Wh %d, ActivePower %d", energy, energy / 3600, mgos_hlw8012_readActivePower(hlw8012)));

    if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
      send_ocpp_meter_values();
    }
  } else {
    LOG(LL_INFO, ("Reconnecting to OCPP Backend"));
    connect_ocpp_backend();
  }

  if (mgos_mqtt_global_is_connected()) {
    send_mqtt_topics();
  }

  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
#ifdef MGOS_HAVE_OTA_COMMON
  if (mgos_ota_is_first_boot()) {
    LOG(LL_INFO, ("Performing cleanup"));
    // In case we're upgrading from stock fw, remove its files
    // with the exception of hwinfo_struct.json.
    remove("cert.pem");
    remove("passwd");
    remove("relaydata");
  }
#endif
  if ((hlw8012 = mgos_hlw8012_create()) == NULL) {
    LOG(LL_ERROR, ("Unable to initialize HLW8012"));
  }

  LOG(LL_INFO, ("Starting Wallbox"));

  mgos_hlw8012_begin(hlw8012,
                     mgos_sys_config_get_gpio_cf(),
                     mgos_sys_config_get_gpio_cf1(),
                     mgos_sys_config_get_gpio_sel(),
                     LOW,
                     true,
                     2000);
  mgos_hlw8012_setResistors(hlw8012, 0.001, 5 * 470000, 1000);
  mgos_hlw8012_setCurrentMultiplier(hlw8012, 25.7400);
  mgos_hlw8012_setVoltageMultiplier(hlw8012, 313.4000);
  mgos_hlw8012_setPowerMultiplier(hlw8012, 3414.2900);

  mgos_set_timer(60000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);

  mgos_gpio_set_mode(mgos_sys_config_get_gpio_relay(), MGOS_GPIO_MODE_OUTPUT);

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 1);
  } else {
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);
  }

  mgos_sys_config_set_ocpp_transaction_reset_consumption(mgos_sys_config_get_ocpp_transaction_consumption());
  mgos_sys_config_save(&mgos_sys_config, false, NULL);

  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.GetInfo", "", wallbox_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reboot", "", wallbox_reboot_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Wallbox.Reset", "", wallbox_reset_handler, NULL);

  // MQTT setup and announce
  sprintf(mqtt_announce_topic, "wallbox/%s/announce", mgos_sys_config_get_device_id());
  sprintf(mqtt_state_topic, "wallbox/%s/state", mgos_sys_config_get_device_id());
  sprintf(mqtt_system_topic, "wallbox/%s/system", mgos_sys_config_get_device_id());
  if (mgos_mqtt_global_is_connected()) {
    send_mqtt_topics();
  }

  connect_ocpp_backend();
  time(&last_ocpp_interaction);
  return MGOS_APP_INIT_SUCCESS;
}