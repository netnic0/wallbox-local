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

#include "wb_ocpp.h"
#include "wb_mqtt.h"
#include "wb_power.h"
#include "wb_util.h"

#include "mgos.h"
#include "mgos_ota_http_client.h"
#include "mgos_provision.h"

#define OCPP_STATUS_AVAILABLE "Available"
#define OCPP_STATUS_CHARGING "Charging"
#define OCPP_STATUS_FINISHING "Finishing"
#define OCPP_STATUS_PREPARING "Preparing"

#define OCPP_STATUS_ACCEPTED "Accepted"
#define OCPP_STATUS_PENDING "Pending"

#define OCPP_RESET_TYPE_HARD "Hard"
#define OCPP_RESET_TYPE_SOFT "Soft"

#define OCPP_STOP_TRANSACTION_REASON_REMOTE "Remote"
#define OCPP_STOP_TRANSACTION_REASON_SOFTRESET "SoftReset"
#define OCPP_STOP_TRANSACTION_REASON_HARDRESET "HardReset"
#define OCPP_STOP_TRANSACTION_REASON_OTHER "Other"

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
    "{\"key\":\"IntensityLimit\",\"readonly\":false,\"value\":\"%d\"},"
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

const char *WS_HEADER = "x-forwarded-for: %s\r\n";

static bool ws_connected = false;
static bool registered = false;
static char *tag_id = NULL;
static char start_transaction_uuid[50];
static char stop_transaction_uuid[50];
static char boot_notification_uuid[50];
static char default_uuid[50];
static struct mg_connection *ws_connection;
static time_t last_ocpp_interaction;

static void safe_free(void *ptr) {
  if (ptr != NULL) {
    free(ptr);
    ptr = NULL;
  }
}

void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  switch (ev) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        LOG(LL_ERROR, ("Connection error: %d", status));
      }
      break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
      LOG(LL_DEBUG, ("Handshake request"));
      break;
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      struct http_message *hm = (struct http_message *) ev_data;
      if (ws_connected) {
        LOG(LL_DEBUG, ("Already connected"));
        break;
      }
      if (hm->resp_code == 101) {
        LOG(LL_INFO, ("Connected to CS"));
        mgos_provision_set_cur_state(MGOS_PROVISION_ST_CLOUD_CONNECTED);
        ws_connected = true;
        if (!registered) {
          ocpp_send_boot_notification();
        }
      } else {
        LOG(LL_ERROR, ("Connection failed [%d]", hm->resp_code));
      }
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      LOG(LL_VERBOSE_DEBUG, ("Frame %.*s", (int) wm->size, wm->data));
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
          ocpp_handle_ocpp_cmd(nc, cmd, uuid, payload);
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
          ocpp_handle_ocpp_response(nc, uuid, payload);
        } else {
          break;
        }
      }
      break;
    }
    case MG_EV_CLOSE: {
      LOG(LL_INFO, ("Disconnection from CS"));
      ws_connected = false;
      break;
    }
  }
  (void) user_data;
}

bool ocpp_is_connected() {
  return ws_connected && registered;
}

void ocpp_synchronize() {
  power_update();
  if (!ws_connected) {
    LOG(LL_INFO, ("Reconnecting to CS"));
    ocpp_connect_backend();
  } else if (registered) {
    ocpp_send_heartbeat();
    if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
      ocpp_update_transaction();
    }
  } else {
    ocpp_send_boot_notification();
  }
}

void ocpp_connect_backend() {
  if (mgos_sys_config_get_ocpp_url() != NULL && mgos_sys_config_get_ocpp_name() != NULL) {
    int urlLength = strlen(mgos_sys_config_get_ocpp_url());
    int nameLength = strlen(mgos_sys_config_get_ocpp_name());

    char buf[urlLength + nameLength + 2];
    sprintf(buf, "%.*s/%.*s", urlLength, mgos_sys_config_get_ocpp_url(), nameLength, mgos_sys_config_get_ocpp_name());

    LOG(LL_INFO, ("Connecting to CS"));

    char extraHeaders[128];
    char ip[25];
    get_chargepoint_ip_address(ip);
    sprintf(extraHeaders, WS_HEADER, ip);
    ws_connection = mg_connect_ws(mgos_get_mgr(), ev_handler, NULL, buf, "ocpp1.6", extraHeaders);
    time(&last_ocpp_interaction);
  } else {
    LOG(LL_WARN, ("Invalid OCPP config"));
  }
}

void ocpp_send_ocpp_response(struct mg_connection *nc, const char *id, const struct mg_str data) {
  int length;
  char buf[(int) data.len + 100];

  length = sprintf(buf, "[3, \"%s\", %.*s]", id, (int) data.len, data.p);
  LOG(LL_DEBUG, ("Sending OCPP response (%d b): %.*s", length, length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);

  time(&last_ocpp_interaction);
}

void ocpp_send_ocpp_request(struct mg_connection *nc, const char *cmd, const char *id, const struct mg_str data) {
  if (!registered && strcmp(cmd, OCPP_REQUEST_BOOT_NOTIFICATION) != 0) {
    LOG(LL_WARN, ("Sending OCPP request %s while unregistered, canceling", cmd));
    return;
  }
  int length;
  char buf[(int) data.len + 100];

  length = sprintf(buf, "[2, \"%s\", \"%s\", %.*s]", id, cmd, (int) data.len, data.p);
  LOG(LL_DEBUG, ("Sending OCPP request (%d b): %.*s", length, length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);

  time(&last_ocpp_interaction);
}

void ocpp_send_boot_notification() {
  char sn[25];
  get_chargepoint_serial_number(sn);
  char buf[1024];
  int length = sprintf(buf,
                       OCPP_BOOTNOTIFICATION,
                       mgos_sys_ro_vars_get_app(),
                       sn,
                       mgos_sys_ro_vars_get_fw_version(),
                       mgos_sys_ro_vars_get_fw_timestamp());
  struct mg_str content = mg_mk_str_n(buf, length);
  generate_uuid(boot_notification_uuid);
  LOG(LL_DEBUG, ("Sending boot notification %.*s", length, buf));
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_BOOT_NOTIFICATION, boot_notification_uuid, content);
}

void send_heartbeat() {
  generate_uuid(default_uuid);
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_HEARTBEAT, default_uuid, mg_mk_str("{}"));
}

void ocpp_send_heartbeat(bool may_skip /* = true */) {
  if (!may_skip) {
    send_heartbeat();
    return;
  }

  time_t now;
  time(&now);
  int interval = mgos_sys_config_get_ocpp_config_heartbeat_interval();
  double diff = difftime(now, last_ocpp_interaction);
  if (diff >= interval) {
    send_heartbeat();
  }
}

void ocpp_send_status_notification(const char *status) {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);

  length = sprintf(buf, OCPP_STATUSNOTIFICATION, status, date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending status notification %.*s", length, buf));
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_STATUS_NOTIFICATION, default_uuid, content);
}

void ocpp_update_transaction() {
  if (mgos_sys_config_get_ocpp_transaction_intensity() > mgos_sys_config_get_ocpp_config_intensity_limit()) {
    LOG(LL_ERROR,
        ("Intensity %d higher than limit %d, stopping transaction",
         mgos_sys_config_get_ocpp_transaction_intensity(),
         mgos_sys_config_get_ocpp_config_intensity_limit()));
    ocpp_stop_transaction(OCPP_STOP_TRANSACTION_REASON_OTHER);
  } else {
    ocpp_send_meter_values();
  }
}

void ocpp_send_meter_values() {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);

  length = sprintf(buf,
                   OCPP_METERVALUES,
                   mgos_sys_config_get_ocpp_transaction_id(),
                   mgos_sys_config_get_ocpp_transaction_consumption(),
                   date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending meter values %.*s", length, buf));
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_METER_VALUES, default_uuid, content);
}

void ocpp_send_stop_transaction(const char *reason) {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(stop_transaction_uuid);

  length = sprintf(buf,
                   OCPP_STOPTRANSACTION,
                   mgos_sys_config_get_ocpp_transaction_consumption(),
                   mgos_sys_config_get_ocpp_transaction_id(),
                   mgos_sys_config_get_ocpp_transaction_tag_id(),
                   date_buffer,
                   reason);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending stop transaction %.*s", length, buf));
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_STOP_TRANSACTION, stop_transaction_uuid, content);
}

void ocpp_send_start_transaction() {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(start_transaction_uuid);

  length = sprintf(buf, OCPP_STARTTRANSACTION, tag_id, date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_DEBUG, ("Sending start transaction %.*s", length, buf));
  ocpp_send_ocpp_request(ws_connection, OCPP_REQUEST_START_TRANSACTION, start_transaction_uuid, content);
}

mg_str ocpp_stop_transaction(const char *reason) {
  LOG(LL_INFO,
      ("Stopping transaction %d for tag %s, reason %s",
       mgos_sys_config_get_ocpp_transaction_id(),
       mgos_sys_config_get_ocpp_transaction_tag_id(),
       reason));

  // This status is not required and it conflicts with the stop transactions since the last modifications on emobility
  // ocpp_send_status_notification(OCPP_STATUS_FINISHING);
  // see https://github.com/sap-labs-france/ev-server/pull/2522
  mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 0);

  power_update();

  ocpp_send_stop_transaction(reason);

  if (mqtt_is_connected()) {
    mqtt_send_state_topic();
  }

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

mg_str ocpp_stop_transaction(const char *payload, const char *reason) {
  int id = 0;

  if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &id) > 0) {
    if (id == mgos_sys_config_get_ocpp_transaction_id()) {
      return ocpp_stop_transaction(reason);
    } else {
      LOG(LL_ERROR,
          ("Payload does not match current transaction %d: %s", mgos_sys_config_get_ocpp_transaction_id(), payload));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);
    }
  } else {
    LOG(LL_ERROR, ("Transaction ID not found in payload: %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

mg_str ocpp_start_transaction(const char *payload) {
  if (json_scanf(payload, strlen(payload), "{ idTag:%Q }", &tag_id) > 0) {
    LOG(LL_INFO, ("Starting transaction for tag %s", tag_id));

    ocpp_send_status_notification(OCPP_STATUS_PREPARING);
    ocpp_send_start_transaction();

    if (mqtt_is_connected()) {
      mqtt_send_state_topic();
    }

    return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
  } else {
    LOG(LL_WARN, ("Tag ID not found in payload: %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

mg_str ocpp_reset_soft() {
  LOG(LL_INFO, ("Performing soft reset"));

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    ocpp_stop_transaction(OCPP_STOP_TRANSACTION_REASON_SOFTRESET);
  }

  mqtt_reset();

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

mg_str ocpp_reset_hard() {
  LOG(LL_INFO, ("Performing hard reset"));

  if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
    ocpp_stop_transaction(OCPP_STOP_TRANSACTION_REASON_HARDRESET);
  }

  mgos_system_restart_after(10000);

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

mg_str ocpp_reset(const char *payload) {
  char *reset_type = NULL;

  if (json_scanf(payload, strlen(payload), "{ type:%Q }", &reset_type) > 0) {
    LOG(LL_INFO, ("Resetting in mode %s", reset_type));

    if (strcmp(OCPP_RESET_TYPE_SOFT, reset_type) == 0) {
      safe_free(reset_type);
      return ocpp_reset_soft();
    } else if (strcmp(OCPP_RESET_TYPE_HARD, reset_type) == 0) {
      safe_free(reset_type);
      return ocpp_reset_hard();
    } else {
      LOG(LL_WARN, ("Reset mode %s not supported", reset_type));
      safe_free(reset_type);
      return mg_mk_str(OCPP_RESPONSE_REJECTED);
    }
  }

  LOG(LL_WARN, ("Reset mode not found in payload: %s", payload));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

void ocpp_get_configuration(const char *payload, char *response) {
  LOG(LL_DEBUG, ("OCPP GetConfiguration request: %.*s", sizeof(payload), payload));

  sprintf(response,
          OCPP_CONFIGURATION,
          mgos_sys_config_get_ocpp_url(),
          mgos_sys_config_get_ocpp_name(),
          mgos_sys_config_get_ocpp_config_intensity_limit(),
          mgos_sys_config_get_ocpp_config_heartbeat_interval());
}

mg_str ocpp_change_configuration(const char *payload) {
  LOG(LL_DEBUG, ("OCPP ChangeConfiguration request: %s", payload));
  char *key = NULL;

  if (json_scanf(payload, strlen(payload), "{ key:%Q }", &key) > 0) {
    if (strcasecmp("HeartbeatInterval", key) == 0) {
      int value;
      if (json_scanf(payload, strlen(payload), "{ value:%d }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%d\"", key, value));
        if (value >= 60) {
          const int currentValue = mgos_sys_config_get_ocpp_config_heartbeat_interval();
          if (value != currentValue) {
            mgos_sys_config_set_ocpp_config_heartbeat_interval(value);
            mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          }
          safe_free(key);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
        LOG(LL_ERROR, ("ChangeConfiguration request with incorrect value for key: \"%s\", value \"%d\"", key, value));
        safe_free(key);
        return mg_mk_str(OCPP_RESPONSE_REJECTED);
      }
      LOG(LL_ERROR, ("ChangeConfiguration request without number value for key: \"%s\"", key));
      safe_free(key);
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else if (strcasecmp("OCPPCentralAddress", key) == 0) {
      char *value = NULL;
      if (json_scanf(payload, strlen(payload), "{ value:%Q }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%s\"", key, value));
        if (value != NULL && strlen(value) <= 500) {
          const char *currentValue = mgos_sys_config_get_ocpp_url();
          if (strcmp(value, currentValue) != 0) {
            mgos_sys_config_set_ocpp_url(value);
            mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          }
          safe_free(key);
          safe_free(value);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
      }
      LOG(LL_WARN, ("ChangeConfiguration request without value for key: \"%s\"", key));
      safe_free(key);
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else if (strcasecmp("StationName", key) == 0) {
      char *value = NULL;
      if (json_scanf(payload, strlen(payload), "{ value:%Q }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%s\"", key, value));
        if (value != NULL && strlen(value) <= 500) {
          const char *currentValue = mgos_sys_config_get_ocpp_name();
          if (strcmp(value, currentValue) != 0) {
            mgos_sys_config_set_ocpp_name(value);
            mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          }
          safe_free(key);
          safe_free(value);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
      }
      LOG(LL_WARN, ("ChangeConfiguration request without value for key: \"%s\"", key));
      safe_free(key);
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else if (strcasecmp("IntensityLimit", key) == 0) {
      int value;
      if (json_scanf(payload, strlen(payload), "{ value:%d }", &value) > 0) {
        LOG(LL_INFO, ("Change configuration key \"%s\", value \"%d\"", key, value));
        const int currentValue = mgos_sys_config_get_ocpp_config_intensity_limit();
        if (value != currentValue && value > 0 && value <= 16) {
          mgos_sys_config_set_ocpp_config_intensity_limit(value);
          mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
          return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
        }
        LOG(LL_ERROR, ("ChangeConfiguration request with incorrect value for key: \"%s\", value \"%d\"", key, value));
        return mg_mk_str(OCPP_RESPONSE_REJECTED);
      }
      LOG(LL_ERROR, ("ChangeConfiguration request without number value for key: \"%s\"", key));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);

    } else {
      LOG(LL_ERROR, ("ChangeConfiguration request for unsupported key: \"%s\"", key));
      safe_free(key);
      return mg_mk_str(OCPP_RESPONSE_NOTSUPPORTED);
    }
  }
  LOG(LL_ERROR, ("No key for ChangeConfiguration request"));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

mg_str ocpp_update_firmware(const char *payload) {
  char *location = NULL;

  if (json_scanf(payload, strlen(payload), "{ location:%Q }", &location) > 0) {
    LOG(LL_INFO, ("Updating firmware from %s", location));
    mgos_ota_http_start(location, NULL);

    safe_free(location);
    return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
  }

  LOG(LL_WARN, ("Location not found in payload: %s", payload));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

void ocpp_handle_response(const char *id, const char *payload) {
  LOG(LL_DEBUG, ("Unhandled OCPP response with ID %s, payload: %s", id, payload));
}

void ocpp_handle_response_start_transaction(const char *id, const char *payload) {
  int transaction_id;

  LOG(LL_DEBUG, ("Handle OCPP %s response with ID %s, payload: %s", OCPP_REQUEST_START_TRANSACTION, id, payload));
  if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &transaction_id) > 0) {
    ocpp_send_status_notification(OCPP_STATUS_CHARGING);
    power_reset_energy();
    mgos_gpio_write(mgos_sys_config_get_gpio_relay(), 1);
    mgos_sys_config_set_ocpp_transaction_id(transaction_id);
    mgos_sys_config_set_ocpp_transaction_tag_id(tag_id);
    mgos_sys_config_set_ocpp_transaction_uptime(mgos_uptime());
    mgos_sys_config_set_ocpp_transaction_consumption(0);
    mgos_sys_config_set_ocpp_transaction_intensity(0);
    mgos_sys_config_set_ocpp_transaction_reset_consumption(0);
    mgos_sys_config_save(&mgos_sys_config, false, NULL);
    LOG(LL_INFO, ("Transaction started, ID: %d", transaction_id));
  } else {
    ocpp_send_status_notification(OCPP_STATUS_AVAILABLE);
    mgos_sys_config_set_ocpp_transaction_id(-1);
    mgos_sys_config_save(&mgos_sys_config, false, NULL);
    LOG(LL_ERROR, ("Failed to start transaction"));
  }
}

void ocpp_handle_response_stop_transaction(const char *id) {
  LOG(LL_DEBUG, ("Handle OCPP %s response with ID %s", OCPP_REQUEST_STOP_TRANSACTION, id));
  ocpp_send_status_notification(OCPP_STATUS_AVAILABLE);
  power_reset_energy();
  mgos_sys_config_set_ocpp_transaction_consumption(0);
  mgos_sys_config_set_ocpp_transaction_intensity(0);
  mgos_sys_config_set_ocpp_transaction_reset_consumption(0);
  mgos_sys_config_set_ocpp_transaction_id(-1);
  mgos_sys_config_save(&mgos_sys_config, false, NULL);
  LOG(LL_INFO, ("Transaction stopped"));
}

void ocpp_handle_response_boot_notification(const char *id, const char *payload) {
  char *registration_status;
  int heartbeat_interval;

  if (json_scanf(payload, strlen(payload), "{ status:%Q }", &registration_status) > 0 &&
      strcmp(registration_status, OCPP_STATUS_ACCEPTED) == 0) {
    LOG(LL_DEBUG, ("Handle OCPP %s response with ID %s, payload: %s", OCPP_REQUEST_BOOT_NOTIFICATION, id, payload));
    registered = true;
    if (mgos_sys_config_get_ocpp_transaction_id() > 0) {
      ocpp_send_status_notification(OCPP_STATUS_CHARGING);
    } else {
      ocpp_send_status_notification(OCPP_STATUS_AVAILABLE);
    }
    if (json_scanf(payload, strlen(payload), "{ interval:%d }", &heartbeat_interval) > 0) {
      int interval = mgos_sys_config_get_ocpp_config_heartbeat_interval();
      if (heartbeat_interval >= 60 && heartbeat_interval != interval) {
        mgos_sys_config_set_ocpp_config_heartbeat_interval(heartbeat_interval);
        mgos_sys_config_save_level(&mgos_sys_config, MGOS_CONFIG_LEVEL_USER, false, NULL);
        LOG(LL_INFO, ("Heartbeat interval set to %d (CS request)", heartbeat_interval));
      }
    }
    safe_free(registration_status);
  } else if (json_scanf(payload, strlen(payload), "{ status:%Q }", &registration_status) > 0 &&
             strcmp(registration_status, OCPP_STATUS_PENDING) == 0) {
    LOG(LL_INFO, ("Registration pending on CS"));
    safe_free(registration_status);
  } else {
    LOG(LL_INFO, ("Registration failure on CS"));
  }
}

void ocpp_handle_ocpp_response(struct mg_connection *nc, const char *id, const char *payload) {
  if (strcmp(id, start_transaction_uuid) == 0) {
    ocpp_handle_response_start_transaction(id, payload);
  } else if (strcmp(id, stop_transaction_uuid) == 0) {
    ocpp_handle_response_stop_transaction(id);
  } else if (strcmp(id, boot_notification_uuid) == 0) {
    ocpp_handle_response_boot_notification(id, payload);
  } else {
    ocpp_handle_response(id, payload);
  }

  (void) nc;
}

void ocpp_handle_ocpp_cmd(struct mg_connection *nc, const char *cmd, const char *id, const char *payload) {
  if (!registered) {
    LOG(LL_WARN, ("Received an OCPP command %s while unregistered", cmd));
  }
  LOG(LL_DEBUG, ("Handle OCPP cmd %s with ID %s", cmd, id));
  struct mg_str data;

  if (strcmp(cmd, OCPP_REQUEST_GET_CONFIGURATION) == 0) {
    char resp[3000];
    ocpp_get_configuration(payload, resp);
    data = mg_mk_str(resp);
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

  ocpp_send_ocpp_response(nc, id, data);
}
