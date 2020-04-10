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
#include "mgos_rpc.h"
#include "mgos_hlw8012.h"
#ifdef MGOS_HAVE_OTA_COMMON
#include "mgos_ota.h"
#endif

#ifndef MGOS_HAVE_WIFI
const char *mgos_sys_config_get_wifi_sta_ssid(void)
{
  return "";
}
const char *mgos_sys_config_get_wifi_sta_pass(void)
{
  return "";
}
bool mgos_sys_config_get_wifi_sta_enable(void)
{
  return false;
}
#endif

static struct HLW8012 *hlw8012 = NULL;

static void timer_cb(void *arg)
{
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long)mgos_get_heap_size(),
       (unsigned long)mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
#ifdef LED_PIN
  mgos_gpio_toggle(LED_PIN);
#endif
  (void)arg;
}

static void shelly_get_info_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg, struct mg_rpc_frame_info *fi,
                                    struct mg_str args)
{
  mg_rpc_send_responsef(
      ri,
      "{id: %Q, app: %Q, version: %Q, fw_build: %Q, current: %d, voltage: %d, energy: %d, "
      "sw1: {id: %d, name: %Q, in_mode: %d, persist: %B, state: %B}"
      "} ",
      mgos_sys_config_get_device_id(), MGOS_APP,
      mgos_sys_ro_vars_get_fw_version(), mgos_sys_ro_vars_get_fw_id(),
      mgos_hlw8012_readCurrent(hlw8012), mgos_hlw8012_readVoltage(hlw8012), mgos_hlw8012_readEnergy(hlw8012),
      mgos_sys_config_get_sw1_id(), mgos_sys_config_get_sw1_name(),
      mgos_sys_config_get_sw1_in_mode(),
      mgos_sys_config_get_sw1_persist_state(), mgos_gpio_read(mgos_sys_config_get_sw1_in_gpio()));
  (void)cb_arg;
  (void)fi;
  (void)args;
}

static void shelly_set_switch_handler(struct mg_rpc_request_info *ri,
                                      void *cb_arg,
                                      struct mg_rpc_frame_info *fi,
                                      struct mg_str args)
{
  bool currentValue = mgos_gpio_read(mgos_sys_config_get_sw1_in_gpio());
  mgos_gpio_write(mgos_sys_config_get_sw1_out_gpio(), !currentValue);
  mg_rpc_send_responsef(ri, "{currentValue: %B, newValue: %B}", currentValue, !currentValue);

  (void)cb_arg;
  (void)fi;
  (void)args;
}

static void shelly_get_conso_handler(struct mg_rpc_request_info *ri,
                                     void *cb_arg, struct mg_rpc_frame_info *fi,
                                     struct mg_str args)
{
  mg_rpc_send_responsef(
      ri,
      "{status: %Q,Current: %d, Voltage: %d, Energy: %d, ActivePower: %d, ApparentPower: %d, PowerFactor: %d, ReactivePower: %d}", (hlw8012 == NULL) ? "failed" : "ok",
      mgos_hlw8012_readCurrent(hlw8012),
      mgos_hlw8012_readVoltage(hlw8012),
      mgos_hlw8012_readEnergy(hlw8012),
      mgos_hlw8012_readActivePower(hlw8012),
      mgos_hlw8012_readApparentPower(hlw8012),
      mgos_hlw8012_readPowerFactor(hlw8012),
      mgos_hlw8012_readReactivePower(hlw8012));
  (void)cb_arg;
  (void)fi;
  (void)args;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data)
{
  switch (ev)
  {
  case MG_EV_CONNECT:
  {
    int status = *((int *)ev_data);
    LOG(LL_INFO, ("-- Connection status: %d\n", status));
    if (status != 0)
    {
      LOG(LL_ERROR, ("-- Connection error: %d\n", status));
    }
    break;
  }
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
  {
    struct http_message *hm = (struct http_message *)ev_data;
    if (hm->resp_code == 101)
    {
      LOG(LL_INFO, ("-- Connected\n"));
      struct mg_str content = mg_mk_str("[2,\"1212121\",\"BootNotification\",{\"chargeBoxSerialNumber\": \"EV.534150204C616273204672616E6365\",\"chargePointModel\": \"SHELLY\",\"chargePointSerialNumber\": \"3N4453686F70204361656E\",\"chargePointVendor\": \"SAP Labs DShop Caen\",\"firmwareVersion\": \"0.0.1\"}]");
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, content.p, content.len);
    }
    else
    {
      LOG(LL_ERROR, ("-- Connection failed! HTTP code %d\n", hm->resp_code));
      /* Connection will be closed after this. */
    }
    break;
  }
  case MG_EV_WEBSOCKET_FRAME:
  {
    struct websocket_message *wm = (struct websocket_message *)ev_data;
    LOG(LL_INFO, ("%.*s\n", (int)wm->size, wm->data));
    break;
  }
  case MG_EV_CLOSE:
  {
    LOG(LL_INFO, ("-- WS Disconnection\n"));
    break;
  }
  }
  (void)nc;
  (void)user_data;
}

static void connect_ocpp_backend()
{
  LOG(LL_INFO, ("Connecting to OCPP Backend"));
  struct mg_mgr *mgr = mgos_get_mgr();
  mg_connect_ws(mgr, ev_handler, NULL, "wss://sap-ev-chargebox-json-server-qa.cfapps.eu10.hana.ondemand.com/OCPP16/5c1d018887ea6e000856511a/5d5a8df36b8d26000682edb0/Shelly", "ocpp1.6", NULL);
}

enum mgos_app_init_result mgos_app_init(void)
{
#ifdef MGOS_HAVE_OTA_COMMON
  if (mgos_ota_is_first_boot())
  {
    LOG(LL_INFO, ("Performing cleanup"));
    // In case we're uograding from stock fw, remove its files
    // with the exception of hwinfo_struct.json.
    remove("cert.pem");
    remove("passwd");
    remove("relaydata");
  }
#endif
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif
  if ((hlw8012 = mgos_hlw8012_create()) == NULL)
  {
    LOG(LL_INFO, ("Unable to initialize HLW8012"));
  }
  mgos_hlw8012_begin(hlw8012, mgos_sys_config_get_cf_pin(), mgos_sys_config_get_cf1_pin(), mgos_sys_config_get_sel_pin(), LOW, false, 1000000);
  mgos_hlw8012_setCurrentMultiplier(hlw8012, 25740.0);
  mgos_hlw8012_setVoltageMultiplier(hlw8012, 313400.0);
  mgos_hlw8012_setPowerMultiplier(hlw8012, 3414290.0);

  mgos_set_timer(60000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  mgos_gpio_set_mode(mgos_sys_config_get_sw1_out_gpio(), MGOS_GPIO_MODE_OUTPUT);

  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetInfo", "", shelly_get_info_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.GetConso", "", shelly_get_conso_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "Shelly.SetSwitch", "", shelly_set_switch_handler, NULL);

  connect_ocpp_backend();

  return MGOS_APP_INIT_SUCCESS;
}
