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
#include "mgos_rpc.h"
#ifdef MGOS_HAVE_OTA_COMMON
#include "mgos_ota.h"
#endif

#define OCPP_STATUS_AVAILABLE "Available"
#define OCPP_STATUS_CHARGING "Charging"
#define OCPP_STATUS_FINISHING "Finishing"
#define OCPP_STATUS_PREPARING "Preparing"

#define OCPP_RESET_TYPE_HARD "Hard"
#define OCPP_RESET_TYPE_SOFT "Soft"

#define OCPP_STOP_TRANSACTION_REASON_REMOTE "Remote"
#define OCPP_STOP_TRANSACTION_REASON_SOFTRESET "SoftReset"
#define OCPP_STOP_TRANSACTION_REASON_HARDRESET "HardReset"

#define OCPP_RESPONSE_ACCEPTED "{\"status\": \"Accepted\"}"
#define OCPP_RESPONSE_REJECTED "{\"status\": \"Rejected\"}"

#define OCPP_REQUEST_BOOT_NOTIFICATION "BootNotification"
#define OCPP_REQUEST_GET_CONFIGURATION "GetConfiguration"
#define OCPP_REQUEST_METER_VALUES "MeterValues"
#define OCPP_REQUEST_REMOTE_START_TRANSACTION "RemoteStartTransaction"
#define OCPP_REQUEST_REMOTE_STOP_TRANSACTION "RemoteStopTransaction"
#define OCPP_REQUEST_START_TRANSACTION "StartTransaction"
#define OCPP_REQUEST_STOP_TRANSACTION "StopTransaction"
#define OCPP_REQUEST_HEARTBEAT "Heartbeat"
#define OCPP_REQUEST_STATUS_NOTIFICATION "StatusNotification"
#define OCPP_REQUEST_RESET "Reset"

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
static bool ws_charging = false;
static char *tag_id = NULL;
static char start_transaction_uuid[50];
static char stop_transaction_uuid[50];
static char default_uuid[50];
static int transaction_id = 0;
static struct mg_connection *ws_connection;

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

static void shelly_get_info_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg,
                                    struct mg_rpc_frame_info *fi,
                                    struct mg_str args) {
  mg_rpc_send_responsef(
      ri,
      "{id: %Q, app: %Q, version: %Q, fw_build: %Q, wifi_ssid: %Q, energy: %d, power: %d, state: %B, ocpp_url: %Q, ocpp_name: %Q}",
      mgos_sys_config_get_device_id(),
      MGOS_APP,
      mgos_sys_ro_vars_get_fw_version(),
      mgos_sys_ro_vars_get_fw_id(),
      mgos_sys_config_get_wifi_sta_ssid(),
      mgos_hlw8012_readEnergy(hlw8012),
      mgos_hlw8012_readActivePower(hlw8012),
      mgos_gpio_read(mgos_sys_config_get_sw1_out_gpio()),
      mgos_sys_config_get_ocpp_url(),
      mgos_sys_config_get_ocpp_name());
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void shelly_set_switch_handler(struct mg_rpc_request_info *ri,
                                      void *cb_arg,
                                      struct mg_rpc_frame_info *fi,
                                      struct mg_str args) {
  bool currentValue = mgos_gpio_read(mgos_sys_config_get_sw1_out_gpio());
  mgos_gpio_toggle(mgos_sys_config_get_sw1_out_gpio());
  mg_rpc_send_responsef(ri, "{currentValue: %B, newValue: %B}", currentValue, !currentValue);

  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void shelly_get_conso_handler(struct mg_rpc_request_info *ri,
                                     void *cb_arg,
                                     struct mg_rpc_frame_info *fi,
                                     struct mg_str args) {
  mg_rpc_send_responsef(
      ri,
      "{status: %Q,Current: %d, Voltage: %d, Energy: %d, ActivePower: %d, ApparentPower: %d, PowerFactor: %d, ReactivePower: %d}",
      (hlw8012 == NULL) ? "failed" : "ok",
      mgos_hlw8012_readCurrent(hlw8012),
      mgos_hlw8012_readVoltage(hlw8012),
      mgos_hlw8012_readEnergy(hlw8012),
      mgos_hlw8012_readActivePower(hlw8012),
      mgos_hlw8012_readApparentPower(hlw8012),
      mgos_hlw8012_readPowerFactor(hlw8012),
      mgos_hlw8012_readReactivePower(hlw8012));
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void shelly_get_uid_handler(struct mg_rpc_request_info *ri,
                                   void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  generate_uuid(default_uuid);
  mg_rpc_send_responsef(ri, "{uid: %Q}", default_uuid);
  (void) cb_arg;
  (void) fi;
  (void) args;
}

static void send_ocpp_response(struct mg_connection *nc, const char *id, struct mg_str data) {
  char buf[1024];
  int length;
  length = sprintf(buf, "[3, \"%s\", %s]", id, data.p);
  LOG(LL_INFO, ("Sending response %.*s", length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);
}

static void send_ocpp_request(struct mg_connection *nc, const char *cmd, const char *id, struct mg_str data) {
  char buf[1024];
  int length;
  length = sprintf(buf, "[2, \"%s\", \"%s\", %s]", id, cmd, data.p);
  LOG(LL_INFO, ("Sending request %.*s", length, buf));
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, length);
}

static void get_current_date(char *buffer) {
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // 2020-04-13T11:39:35.116Z
  strftime(buffer, 21, "%FT%TZ", timeinfo);
}

static void send_ocpp_heartbeat() {
  generate_uuid(default_uuid);
  send_ocpp_request(ws_connection, OCPP_REQUEST_HEARTBEAT, default_uuid, mg_mk_str("{}"));
}

static void send_ocpp_status_notification(const char *status) {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);

  length = sprintf(buf,
                   "{\"connectorId\": 1,\"errorCode\": \"NoError\",\"status\": \"%s\",\"timestamp\": \"%s\"}",
                   status,
                   date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_INFO, ("Sending status notification %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_STATUS_NOTIFICATION, default_uuid, content);
}

static void send_ocpp_meter_values() {
  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(default_uuid);
  int energy = mgos_hlw8012_readEnergy(hlw8012) / 3600;

  length = sprintf(
      buf,
      "{\"connectorId\":1,\"transactionId\":%d,\"meterValue\":[{\"sampledValue\":[{\"unit\":\"Wh\",\"context\":\"Sample.Periodic\",\"value\":\"%d\"}],\"timestamp\":\"%s\"}]}",
      transaction_id,
      energy,
      date_buffer);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_INFO, ("Sending meter values %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_METER_VALUES, default_uuid, content);
}

static mg_str stopTransaction(const char *reason) {
  LOG(LL_INFO, ("Stop transaction %d for tag %s, reason %s", transaction_id, tag_id, reason));

  send_ocpp_status_notification(OCPP_STATUS_FINISHING);
  mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), 0);

  char buf[200];
  int length;

  char date_buffer[30];
  get_current_date(date_buffer);
  generate_uuid(stop_transaction_uuid);

  int energy = mgos_hlw8012_readEnergy(hlw8012) / 3600;

  length = sprintf(
      buf,
      "{\"meterStop\": %d,\"transactionId\": \"%d\",\"idTag\": \"%s\",\"timestamp\": \"%s\",\"reason\": \"%s\"}",
      energy,
      transaction_id,
      tag_id,
      date_buffer,
      reason);
  struct mg_str content = mg_mk_str_n(buf, length);
  LOG(LL_INFO, ("Sending stop transaction %.*s", length, buf));
  send_ocpp_request(ws_connection, OCPP_REQUEST_STOP_TRANSACTION, stop_transaction_uuid, content);
  ws_charging = false;
  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

static mg_str stopTransaction(const char *payload, const char *reason) {
  if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &transaction_id) > 0) {
    return stopTransaction(reason);
  } else {
    LOG(LL_INFO, ("Unable to find transaction id in payload %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

static mg_str startTransaction(const char *payload) {
  if (json_scanf(payload, strlen(payload), "{ idTag:%Q }", &tag_id) > 0) {
    LOG(LL_INFO, ("Starting transaction for tag with id %s", tag_id));

    char buf[200];
    int length;

    char date_buffer[30];
    get_current_date(date_buffer);
    generate_uuid(start_transaction_uuid);

    send_ocpp_status_notification(OCPP_STATUS_PREPARING);

    length = sprintf(
        buf, "{\"connectorId\": 1, \"meterStart\": 0, \"idTag\": \"%s\",\"timestamp\": \"%s\"}", tag_id, date_buffer);
    struct mg_str content = mg_mk_str_n(buf, length);
    LOG(LL_INFO, ("Sending start transaction %.*s", length, buf));
    send_ocpp_request(ws_connection, OCPP_REQUEST_START_TRANSACTION, start_transaction_uuid, content);
    return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
  } else {
    LOG(LL_INFO, ("Unable to find tag id in payload %s", payload));
    return mg_mk_str(OCPP_RESPONSE_REJECTED);
  }
}

/*
 * Soft: Return to initial status, gracefully terminating any transactions in progress.
 * At receipt of a soft reset, the Charge Point SHALL return to a state that behaves as just having been booted.
 * If any transaction is in progress it SHALL be terminated normally, before the reset, as in Stop Transaction.
 * Send StatusNotification/ResetFailure if not able to reset.
 */
static mg_str reset_soft() {
  LOG(LL_INFO, ("Performing soft reset"));

  if (ws_charging) {
    stopTransaction(OCPP_STOP_TRANSACTION_REASON_SOFTRESET);
  }

  mgos_app_init();

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

/*
 * Hard: Full reboot of Charge Point software.
 * At receipt of a hard reset the Charge Point SHALL attempt to terminate any transaction in progress normally as
 * in StopTransaction and then perform a reboot.
 * Send StatusNotification/ResetFailure if not able to reset.
 */
static mg_str reset_hard() {
  LOG(LL_INFO, ("Performing hard reset"));

  if (ws_charging) {
    stopTransaction(OCPP_STOP_TRANSACTION_REASON_HARDRESET);
  }

  mgos_system_restart_after(5000);

  return mg_mk_str(OCPP_RESPONSE_ACCEPTED);
}

static mg_str reset(const char *payload) {
  char *reset_type = NULL;

  if (json_scanf(payload, strlen(payload), "{ type:%Q }", &reset_type) > 0) {
    LOG(LL_DEBUG, ("Resetting in mode %s", reset_type));

    if (strcmp(OCPP_RESET_TYPE_SOFT, reset_type) == 0) {
      return reset_soft();
    } else if (strcmp(OCPP_RESET_TYPE_HARD, reset_type) == 0) {
      return reset_hard();
    } else {
      LOG(LL_WARN, ("Reset type %s not supported", reset_type));
      return mg_mk_str(OCPP_RESPONSE_REJECTED);
    }
  }

  LOG(LL_INFO, ("Unable to find reset type in payload %s", payload));
  return mg_mk_str(OCPP_RESPONSE_REJECTED);
}

static void handle_ocpp_response(struct mg_connection *nc, const char *id, const char *payload) {
  LOG(LL_INFO, ("Handle ocpp response with id %s", id));
  if (strcmp(id, start_transaction_uuid) == 0) {
    if (json_scanf(payload, strlen(payload), "{ transactionId:%d }", &transaction_id) > 0) {
      send_ocpp_status_notification(OCPP_STATUS_CHARGING);
      ws_charging = true;
      mgos_hlw8012_resetEnergy(hlw8012);
      mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), 1);
      LOG(LL_INFO, ("Transaction started %d", transaction_id));
    } else {
      send_ocpp_status_notification(OCPP_STATUS_AVAILABLE);
      ws_charging = false;
      transaction_id = -1;
      LOG(LL_INFO, ("Failed to start transaction"));
    }
  } else if (strcmp(id, stop_transaction_uuid) == 0) {
    send_ocpp_status_notification(OCPP_STATUS_AVAILABLE);
    ws_charging = false;
    transaction_id = -1;
    mgos_hlw8012_resetEnergy(hlw8012);
  }

  (void) nc;
}

static void handle_ocpp_cmd(struct mg_connection *nc, const char *cmd, const char *id, const char *payload) {
  LOG(LL_INFO, ("Handle ocpp cmd %s with id %s", cmd, id));
  struct mg_str data;
  if (strcmp(cmd, OCPP_REQUEST_GET_CONFIGURATION) == 0) {
    data = mg_mk_str("{}");
  } else if (strcmp(cmd, OCPP_REQUEST_REMOTE_START_TRANSACTION) == 0) {
    data = startTransaction(payload);
  } else if (strcmp(cmd, OCPP_REQUEST_REMOTE_STOP_TRANSACTION) == 0) {
    data = stopTransaction(payload, OCPP_STOP_TRANSACTION_REASON_REMOTE);
  } else if (strcmp(cmd, OCPP_REQUEST_RESET) == 0) {
    data = reset(payload);
  } else {
    data = mg_mk_str("{}");
  }
  send_ocpp_response(nc, id, data);
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  switch (ev) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      LOG(LL_INFO, ("-- Connection status: %d", status));
      if (status != 0) {
        LOG(LL_ERROR, ("-- Connection error: %d", status));
      }
      break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
      LOG(LL_INFO, ("-- handshake Request"));
      break;
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      struct http_message *hm = (struct http_message *) ev_data;
      if (ws_connected == true) {
        LOG(LL_INFO, ("-- Already Connected"));
        break;
      }
      if (hm->resp_code == 101) {
        LOG(LL_INFO, ("-- Connected"));
        ws_connected = true;
        struct mg_str content = mg_mk_str(
            "{\"chargeBoxSerialNumber\": \"EV.534150204C616273204672616E6365\",\"chargePointModel\": \"SHELLY\",\"chargePointSerialNumber\": \"3N4453686F70204361656E\",\"chargePointVendor\": \"SAP Labs DShop Caen\",\"firmwareVersion\": \"0.0.1\"}");
        send_ocpp_request(nc, OCPP_REQUEST_BOOT_NOTIFICATION, "1212121", content);
      } else {
        LOG(LL_ERROR, ("-- Connection failed! HTTP code %d", hm->resp_code));
        /* Connection will be closed after this. */
      }
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      LOG(LL_INFO, ("-- Frame %.*s", (int) wm->size, wm->data));
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

    ws_connection = mg_connect_ws(mgos_get_mgr(), ev_handler, NULL, buf, "ocpp1.6", NULL);
  } else {
    LOG(LL_WARN, ("OCPP Config is not defined !"));
  }
}

static void timer_cb(void *arg) {
  LOG(LL_INFO, ("Timer callback connected ? %d", ws_connected));
  if (ws_connected == true) {
    send_ocpp_heartbeat();
    int energy = mgos_hlw8012_readEnergy(hlw8012);
    LOG(LL_INFO, ("Energy Ws  %d", energy));
    LOG(LL_INFO, ("Energy Wh %d", energy / 3600));
    LOG(LL_INFO, ("ActivePower %d", mgos_hlw8012_readActivePower(hlw8012)));

    if (ws_charging == true && transaction_id > 0) {
      send_ocpp_meter_values();
    }
  } else {
    LOG(LL_INFO, ("Reconnecting to OCPP Backend"));
    connect_ocpp_backend();
  }
  (void) arg;
}

static void button_handler(int pin, void *arg) {
  LOG(LL_INFO, ("Click from %d !", pin));
  bool currentValue = mgos_gpio_read(mgos_sys_config_get_sw1_out_gpio());
  LOG(LL_INFO, ("Previous Value %d !", currentValue));
  mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), !currentValue);
  bool newValue = mgos_gpio_read(mgos_sys_config_get_sw1_out_gpio());
  LOG(LL_INFO, ("New Value %d !", newValue));
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
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif
  if ((hlw8012 = mgos_hlw8012_create()) == NULL) {
    LOG(LL_INFO, ("Unable to initialize HLW8012"));
  }

  LOG(LL_INFO, ("Starting Shelly Wallbox"));

  mgos_hlw8012_begin(hlw8012,
                     mgos_sys_config_get_cf_pin(),
                     mgos_sys_config_get_cf1_pin(),
                     mgos_sys_config_get_sel_pin(),
                     LOW,
                     true,
                     2000000);
  mgos_hlw8012_setResistors(hlw8012, 0.001, 5 * 470000, 1000);
  mgos_hlw8012_setCurrentMultiplier(hlw8012, 25740.0);
  mgos_hlw8012_setVoltageMultiplier(hlw8012, 313400.0);
  mgos_hlw8012_setPowerMultiplier(hlw8012, 3414290.0);

  mgos_set_timer(60000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);

  mgos_gpio_set_mode(mgos_sys_config_get_sw1_out_gpio(), MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), 0);

  mgos_gpio_set_mode(mgos_sys_config_get_sw1_in_gpio(), MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_set_pull(mgos_sys_config_get_sw1_in_gpio(), MGOS_GPIO_PULL_NONE);
  mgos_gpio_set_button_handler(
      mgos_sys_config_get_sw1_in_gpio(), MGOS_GPIO_PULL_NONE, MGOS_GPIO_INT_EDGE_POS, 100, button_handler, NULL);

  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetInfo", "", shelly_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetConso", "", shelly_get_conso_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.SetSwitch", "", shelly_set_switch_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetUid", "", shelly_get_uid_handler, NULL);

  connect_ocpp_backend();
  return MGOS_APP_INIT_SUCCESS;
}