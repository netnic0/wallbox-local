/*eslint no-alert: "off"*/
/*eslint no-console: "off"*/
require("../assets/main.css");

const host = "";
const WS_POLL_MS = 3000;
const FETCH_POLL_MS = 5000;
const RECONNECT_MIN_MS = 1000;
const RECONNECT_MAX_MS = 30000;
const MAX_WS_FAILURES = 5;

let refreshTimer = null;
let wsReqId = 0;
let wsFailures = 0;
let reconnectDelay = RECONNECT_MIN_MS;
let reconnectTimer = null;

const toast = document.getElementById("toast");

const wifiSSID0 = document.getElementById("wifi_ssid_0");
const wifiPass0 = document.getElementById("wifi_pass_0");
const wifiSSID1 = document.getElementById("wifi_ssid_1");
const wifiPass1 = document.getElementById("wifi_pass_1");
const showWifiPass0 = document.getElementById("wifi_pass_show_0");
const showWifiPass1 = document.getElementById("wifi_pass_show_1");
const deviceState = document.getElementById("state");
const mqttEnable = document.getElementById("mqtt_enable");
const mqttServer = document.getElementById("mqtt_server");
const mqttUser = document.getElementById("mqtt_user");
const mqttPass = document.getElementById("mqtt_pass");
const showMqttPass = document.getElementById("mqtt_pass_show");

const infoContainer = document.getElementById("info_container");
const logsContainer = document.getElementById("logs_container");

const infoSpinner = document.getElementById("info_spinner");
const wifiSpinner = document.getElementById("wifi_spinner");
const mqttSpinner = document.getElementById("mqtt_spinner");
const rebootSpinner = document.getElementById("reboot_spinner");
const resetSpinner = document.getElementById("reset_spinner");
const resetWifiSpinner = document.getElementById("reset_wifi_spinner");
const startSpinner = document.getElementById("start_spinner");
const stopSpinner = document.getElementById("stop_spinner");
const resetEnergySpinner = document.getElementById("reset_energy_spinner");
const firmwareSpinner = document.getElementById("fw_spinner");

const infoTitle = document.getElementById("info_title");
const infoMessage = document.getElementById("info_message");

const showFormControl = (ctrl, show) => {
  if (ctrl) {
    ctrl.style.display = show ? "flex" : "none";
  }
};

const showToast = (message) => {
  toast.innerHTML = message;
  toast.className = "show";
  setTimeout(() => {
    toast.className = toast.className.replace("show", "");
  }, 4000);
};

const handleError = (err) => {
  console.error(err);
  showToast(err && err.message ? err.message : String(err));
};

/* fetch-based JSON-RPC helper. Relies on the browser's native HTTP Digest
   auth: the page itself is served behind http.auth_domain, so cached
   credentials are reused for same-origin /rpc calls (credentials: include). */
const rpc = (method, params) => {
  const opts = {
    method: params ? "POST" : "GET",
    credentials: "include",
  };
  if (params) {
    opts.headers = { "Content-Type": "application/json" };
    opts.body = JSON.stringify(params);
  }
  return fetch(host + "/rpc/" + method, opts).then((res) => {
    if (!res.ok) {
      throw new Error(res.status + " " + res.statusText);
    }
    return res.text().then((text) => (text ? JSON.parse(text) : {}));
  });
};

const TimeLevels = {
  scale: [24, 60, 60, 1],
  units: ["d ", "h ", "m ", "s "],
};

const intervalToLevels = (interval, levels) => {
  const cbFun = (d, c) => {
    let bb = d[1] % c[0],
      aa = (d[1] - bb) / c[0];
    aa = aa > 0 ? aa + c[1] : "";

    return [d[0] + aa, bb];
  };

  let result = levels.scale
    .map((d, i, a) => a.slice(i).reduce((acc, cur) => acc * cur))
    .map((d, i) => [d, levels.units[i]])
    .reduce(cbFun, ["", interval]);
  return result[0];
};

const secondsToString = (interval) => intervalToLevels(interval, TimeLevels);

const stopRefresh = () => {
  if (refreshTimer) {
    clearInterval(refreshTimer);
    refreshTimer = null;
  }
};

/* Render only the live-changing metrics (used by the WS/poll refresh). */
const renderLive = (data) => {
  document.getElementById("energy").innerText =
    (data.energy ? data.energy : 0).toFixed(0) + " Wh";
  document.getElementById("intensity").innerText =
    (data.intensity ? data.intensity : 0).toFixed(0) + " A";
  document.getElementById("power").innerText =
    (data.power ? data.power : 0).toFixed(0) + " W";
  document.getElementById("voltage").innerText =
    (data.voltage ? data.voltage : 0).toFixed(0) + " V";
  document.getElementById("current").innerText =
    (data.current ? data.current : 0).toFixed(2) + " A";
  document.getElementById("uptime").innerText = data.uptime
    ? secondsToString(data.uptime)
    : "-";
  document.getElementById("temperature").innerText =
    (data.temperature ? data.temperature.toFixed(1) : "-") + " °C";
  const mqttServerRO = document.getElementById("mqtt_server_readonly");
  const mqttConnected = document.getElementById("mqtt_connected");
  const chipMqtt = document.getElementById("chip_mqtt");
  const alertMqtt = document.getElementById("alert_mqtt");

  const charging = data.charging === true;
  const ev = !!data.ev;
  deviceState.innerText = charging ? "Charging" : "Available";
  deviceState.className = charging ? "connected" : "";
  const banner = document.getElementById("device_banner");
  if (banner) {
    if (charging) {
      banner.className = "banner banner--ok";
      banner.innerText = ev
        ? "Device: Charging (EV detected)"
        : "Device: Charging";
    } else if (ev) {
      banner.className = "banner banner--warn";
      banner.innerText = "Device: EV detected (relay open)";
    } else {
      banner.className = "banner banner--idle";
      banner.innerText = "Device: Idle";
    }
  }
  if (mqttConnected) {
    mqttConnected.innerText = data.mqtt_connected ? "Yes" : "No";
    mqttConnected.className = data.mqtt_connected ? "status-ok" : "status-bad";
  }
  if (chipMqtt) {
    chipMqtt.className = data.mqtt_connected
      ? "chip chip--ok"
      : "chip chip--bad";
    chipMqtt.innerText = data.mqtt_connected ? "Connected" : "Disconnected";
  }
  if (alertMqtt) {
    alertMqtt.className = data.mqtt_connected ? "alert" : "alert show";
  }
  deviceState.className = charging ? "connected" : "";
  showFormControl(document.getElementById("power-control"), charging);
  showFormControl(document.getElementById("current-control"), charging);
  showFormControl(document.getElementById("energy-control"), charging);
  showFormControl(document.getElementById("intensity-control"), charging);
};

const showInfoDialog = (message, title, spin, reboot) => {
  stopRefresh();
  if (reboot) {
    setTimeout(() => document.location.reload(), 6000);
  }
  infoSpinner.className = spin ? "spin reboot" : "";
  infoContainer.style.display = "block";
  infoTitle.innerHTML = title;
  infoMessage.innerHTML = message;
};

const hideInfoDialog = () => {
  infoContainer.style.display = "none";
};

/* Render the device info fields (static per boot). */
const renderInfo = (data) => {
  document.getElementById("device_id").innerText = data.id;
  document.getElementById("device_sn").innerText = data.sn;
  document.getElementById("device_mac").innerText = data.mac;
  document.getElementById("device_ip").innerText = data.ip;
  document.getElementById("app_name").innerText = data.app;
  document.getElementById("app_version").innerText = data.version;
  document.getElementById("app_build").innerText = data.fw_build;
  document.getElementById("app_date").innerText = data.fw_ts;
  renderLive(data);
  // Update summary when collapsed
  const sumPower = document.getElementById("sum_power");
  const sumEv = document.getElementById("sum_ev");
  const sumTemp = document.getElementById("sum_temp");
  if (sumPower) sumPower.innerText = (data.power != null ? data.power + " W" : "-");
  if (sumEv) sumEv.innerText = (data.ev ? "Yes" : "No");
  if (sumTemp) sumTemp.innerText = (data.temperature != null ? data.temperature.toFixed(1) + " °C" : "-");
};

/* ---- Live data: WebSocket first, fetch-polling fallback ---- */

const startFetchPolling = () => {
  stopRefresh();
  const poll = () => rpc("Wallbox.GetInfo").then(renderLive).catch(handleError);
  poll();
  refreshTimer = setInterval(poll, FETCH_POLL_MS);
};

const scheduleReconnect = () => {
  if (reconnectTimer) {
    return;
  }
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    // eslint-disable-next-line no-use-before-define
    connectWs();
  }, reconnectDelay);
  reconnectDelay = Math.min(reconnectDelay * 2, RECONNECT_MAX_MS);
};

const connectWs = () => {
  stopRefresh();
  if (!window.WebSocket) {
    startFetchPolling();
    return;
  }
  const proto = document.location.protocol === "https:" ? "wss:" : "ws:";
  const url = proto + "//" + document.location.host + "/rpc";
  let socket;
  try {
    socket = new WebSocket(url);
  } catch (e) {
    startFetchPolling();
    return;
  }

  socket.onopen = () => {
    wsFailures = 0;
    reconnectDelay = RECONNECT_MIN_MS;
    stopRefresh();
    const pollWs = () => {
      if (socket.readyState === WebSocket.OPEN) {
        wsReqId += 1;
        socket.send(JSON.stringify({ id: wsReqId, method: "Wallbox.GetInfo" }));
      }
    };
    pollWs();
    refreshTimer = setInterval(pollWs, WS_POLL_MS);
  };

  socket.onmessage = (evt) => {
    try {
      const msg = JSON.parse(evt.data);
      if (msg && msg.result) {
        renderLive(msg.result);
      }
    } catch (e) {
      console.error(e);
    }
  };

  socket.onclose = () => {
    stopRefresh();
    wsFailures += 1;
    if (wsFailures >= MAX_WS_FAILURES) {
      startFetchPolling();
    } else {
      scheduleReconnect();
    }
  };

  socket.onerror = () => socket.close();
};

/* ---- Configuration and action handlers ---- */

const mqttSaveBtn = document.getElementById("mqtt_save_btn");
mqttSaveBtn.onclick = () => {
  mqttSpinner.className = "spin";
  const mqtt = {
    enable: mqttEnable.checked,
    server: mqttServer.value,
  };
  /* Only send the password when the user typed one, otherwise Config.Set
       would overwrite the stored broker password with an empty string
       (GetInfo never returns mqtt.pass, so the field is blank on load). */
  if (mqttPass.value && mqttPass.value.length > 0) {
    mqtt.pass = mqttPass.value;
  }
  /* Only send the username when the user typed one; otherwise keep current. */
  if (
    typeof mqttUser !== "undefined" &&
    mqttUser &&
    mqttUser.value &&
    mqttUser.value.length > 0
  ) {
    mqtt.user = mqttUser.value;
  }
  const data = {
    config: { mqtt: mqtt },
    level: 8,
    save: true,
    reboot: true,
  };
  rpc("Config.Set", data)
    .then(() => {
      showInfoDialog("Device is rebooting.", "Configuration", true, true);
    })
    .catch(handleError)
    .then(() => {
      mqttSpinner.className = "";
    });
};

const wifiSaveBtn = document.getElementById("wifi_save_btn");
wifiSaveBtn.onclick = () => {
  wifiSpinner.className = "spin";
  let valid = false;
  const data = {
    /* eslint-disable */
    config: {
      wifi: {
        sta2: { enable: true },
        ap: { enable: false },
      },
      provision: {
        max_state: 0,
      },
    },
    save: true,
    reboot: true,
    /* eslint-enable */
  };
  if (wifiSSID0.value && wifiSSID0.value.length > 0) {
    data.config.wifi.sta = { enable: true, ssid: wifiSSID0.value };
    valid = true;
    if (wifiPass0.value && wifiPass0.value.length > 0) {
      data.config.wifi.sta.pass = wifiPass0.value;
    }
  }
  if (wifiSSID1.value && wifiSSID1.value.length > 0) {
    data.config.wifi.sta1 = { enable: true, ssid: wifiSSID1.value };
    valid = true;
    if (wifiPass1.value && wifiPass1.value.length > 0) {
      data.config.wifi.sta1.pass = wifiPass1.value;
    }
  }

  if (valid) {
    rpc("Config.Set", data)
      .then(() => {
        showInfoDialog(
          "Device is rebooting and connecting to " +
            wifiSSID0.value +
            ".<br/>" +
            "Connect to the same network and visit <a href='http://wallbox.local/'>wallbox.local</a>.",
          "Configuration",
          false,
          false,
        );
      })
      .catch(handleError)
      .then(() => {
        wifiSpinner.className = "";
      });
  } else {
    wifiSpinner.className = "";
    showToast("Wifi Update failed. Check your data.");
  }
};

const setRelay = (on, spinner) => {
  spinner.className = "spin";
  rpc("Wallbox.SetRelay", { on: on })
    .then(() => {
      showToast(on ? "Charge started" : "Charge stopped");
    })
    .catch(handleError)
    .then(() => {
      spinner.className = "";
    });
};

document.getElementById("start_btn").onclick = () =>
  setRelay(true, startSpinner);
document.getElementById("stop_btn").onclick = () =>
  setRelay(false, stopSpinner);

document.getElementById("reset_energy_btn").onclick = () => {
  if (
    !window.confirm(
      "This will reset the session energy counter.\nDo you want to proceed?",
    )
  ) {
    return;
  }
  resetEnergySpinner.className = "spin";
  rpc("Wallbox.ResetEnergy", {})
    .then(() => {
      showToast("Energy counter reset");
    })
    .catch(handleError)
    .then(() => {
      resetEnergySpinner.className = "";
    });
};

document.getElementById("reset_wifi_btn").onclick = () => {
  if (
    !window.confirm(
      "This action will delete current Wi-Fi configuration \n" +
        "and restart the device in AP mode.\nDo you want to proceed?",
    )
  ) {
    return;
  }
  resetWifiSpinner.className = "spin";
  rpc("Wallbox.ResetWifi", {})
    .then(() => {
      showInfoDialog(
        "Wi-Fi configuration is reset.<br>" +
          "Reconnect to the Wallbox access point to configure the Wi-Fi.",
        "Reset",
        false,
        false,
      );
    })
    .catch(handleError)
    .then(() => {
      resetWifiSpinner.className = "";
    });
};

document.getElementById("reset_btn").onclick = () => {
  if (
    !window.confirm(
      "This action will wipe all user configuration\n" +
        "and reset the device to factory settings.\nDo you want to proceed?",
    )
  ) {
    return;
  }
  resetSpinner.className = "spin";
  rpc("Wallbox.Reset", {})
    .then(() => {
      showInfoDialog(
        "Device configuration is reset.<br>" +
          "Reconnect to the Wallbox access point to configure the Wi-Fi.",
        "Reset",
        false,
        false,
      );
    })
    .catch(handleError)
    .then(() => {
      resetSpinner.className = "";
    });
};

document.getElementById("reboot_btn").onclick = () => {
  if (
    !window.confirm(
      "This action will restart the device.\nDo you want to proceed?",
    )
  ) {
    return;
  }
  rebootSpinner.className = "spin";
  rpc("Wallbox.Reboot", {})
    .then(() => {
      showInfoDialog(
        "Device is rebooting. Please wait...",
        "Reboot",
        true,
        true,
      );
    })
    .catch(handleError)
    .then(() => {
      rebootSpinner.className = "";
    });
};

const getLogs = () => {
  rpc("FS.List")
    .then((data) => {
      if (data && data.length > 0) {
        data.sort();
        data.reverse();
        let logCount = 0;
        data.forEach((filename) => {
          if (filename.startsWith("log_")) {
            const a = document.createElement("a");
            a.appendChild(document.createTextNode(filename));
            a.title = filename;
            a.href = host + "/" + filename;
            a.target = "_blank";
            document.getElementById("logs").appendChild(a);
            logCount += 1;
          }
          logsContainer.style.display = logCount > 0 ? "block" : "none";
        });
      }
    })
    .catch(handleError);
};

/* duplicate renderInfo removed */

const getInfo = () => {
  rpc("Wallbox.GetInfo")
    .then((data) => {
      wifiSSID0.value = data.wifi_ssid;
      wifiSSID1.value = data.wifi_ssid1;
      mqttEnable.checked = data.mqtt_state === true;
      mqttServer.value = data.mqtt_server;
      if (mqttUser) mqttUser.value = ""; // user not exposed by GetInfo (keep blank)
      renderInfo(data);
    })
    .catch(handleError);
};

const updateFirmware = (evt) => {
  evt.preventDefault();
  const selectedFile = document.getElementById("fw_select_file").files[0];
  if (!selectedFile) {
    return;
  }
  firmwareSpinner.className = "spin";
  const formData = new FormData();
  formData.append("file", selectedFile);

  showInfoDialog("Updating firmware, please wait...", "Update", true, false);

  fetch(host + "/update", {
    method: "POST",
    credentials: "include",
    body: formData,
  })
    .then((res) => {
      if (!res.ok) {
        throw new Error(res.status + " " + res.statusText);
      }
      firmwareSpinner.className = "";
      hideInfoDialog();
      showInfoDialog(
        "Firmware update successful.<br>Device is rebooting, please wait...",
        "Update",
        true,
        true,
      );
    })
    .catch((err) => {
      firmwareSpinner.className = "";
      console.error(err);
      hideInfoDialog();
      showToast("Update failed. Check firmware file.");
    });
};

document.getElementById("fw_upload_btn").onclick = updateFirmware;
(function addStyles() {
  const css = `
    .status-ok { color: #0a8; font-weight: 600; }
    .status-bad { color: #c00; font-weight: 600; }
    /* Fallback CSS to ensure collapsing works even if main.css isn't loaded */
    .container.card-collapsed .form-control,
    .container.card-collapsed .form-actions,
    .container.card-collapsed .alert,
    .container.card-collapsed > h2,
    .container.card-collapsed > .banner,
    .container.card-collapsed > p,
    .container.card-collapsed > .flex-list,
    .container.card-collapsed > .footer,
    .container.card-collapsed > .dialog,
    .container.card-collapsed .form-separator { display: none !important; }
    .container.card-collapsed .summary { display: block !important; }
    .container.card-collapsed .form > div > :not(.summary) { display: none !important; }
  `;
  const style = document.createElement('style');
  style.type = 'text/css';
  style.appendChild(document.createTextNode(css));
  document.head.appendChild(style);
})();

document.getElementById("mqtt_reconnect_btn").onclick = () => {
  showToast("Reconnecting MQTT...");
  // Trigger a reconnect by toggling mqtt.enable in-memory via RPC if necessary.
  // If a dedicated RPC existed, it would be used; as a workaround we can disable/enable quickly.
  const current = mqttEnable.checked;
  mqttSpinner.className = "spin";
  // Save current server and credentials; send only non-empty password to avoid overwriting
  const pass = mqttPass && mqttPass.value ? mqttPass.value : undefined;
  const body = {
    config: {
      mqtt: {
        enable: current,
        server: mqttServer.value || "",
        user: mqttUser && mqttUser.value ? mqttUser.value : "",
      },
    },
    save: false,
    reboot: false,
  };
  if (pass !== undefined) {
    body.config.mqtt.pass = pass;
  }
  rpc("Config.Set", body)
    .then(() => {
      showToast("MQTT reconnect requested");
      // Re-fetch info after a short delay
      setTimeout(getInfo, 1500);
    })
    .catch(handleError)
    .then(() => {
      mqttSpinner.className = "";
    });
};

/* (duplicate mqttSaveBtn handler removed; user is handled above) */

const showPassword = (input, cb) => {
  if (input.type === "text") {
    input.type = "password";
    cb.checked = false;
  } else {
    input.type = "text";
    cb.checked = true;
  }
};
showWifiPass0.onclick = () => showPassword(wifiPass0, showWifiPass0);
showWifiPass1.onclick = () => showPassword(wifiPass1, showWifiPass1);
showMqttPass.onclick = () => showPassword(mqttPass, showMqttPass);

(function () {
  getInfo();
  getLogs();
  // Robust: use event delegation for toggles, with debugging
  document.addEventListener('click', function (e) {
    const btn = e.target && e.target.closest ? e.target.closest('[data-toggle="card"]') : null;
    if (!btn) return;
    e.preventDefault();
    const targetId = btn.getAttribute('data-target');
    if (!targetId) {
      console.debug('toggle: missing data-target on', btn);
      return;
    }
    const container = document.getElementById(targetId);
    if (!container) {
      console.debug('toggle: container not found for', targetId);
      return;
    }
    const before = container.className;
    container.classList.toggle('card-collapsed');
    const after = container.className;
    console.debug('toggle:', targetId, 'class:', before, '=>', after);
  }, false);
  connectWs();
})();
