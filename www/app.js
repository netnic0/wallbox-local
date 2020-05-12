/*eslint no-alert: "off"*/
/*eslint no-console: "off"*/
const host = "";
var refreshTimer;

const wifiSSID = document.getElementById("wifi_ssid");
const wifiPass = document.getElementById("wifi_pass");
const ocppUrl = document.getElementById("ocpp_url");
const ocppName = document.getElementById("ocpp_name");
const deviceState = document.getElementById("state");
const ocppState = document.getElementById("ocpp_state");

const infoContainer = document.getElementById("info_container");

const infoSpinner = document.getElementById("info_spinner");
const refreshSpinner = document.getElementById("refresh_spinner");
const wifiSpinner = document.getElementById("wifi_spinner");
const ocppSpinner = document.getElementById("ocpp_spinner");
const rebootSpinner = document.getElementById("reboot_spinner");
const resetSpinner = document.getElementById("reset_spinner");
const firmwareSpinner = document.getElementById("fw_spinner");

const infoTitle = document.getElementById("info_title");
const infoMessage = document.getElementById("info_message");

const showInfoDialog = (message, title, spin, reboot) => {
    clearInterval(refreshTimer);
    if (reboot) {
        setTimeout(function(){ document.location.reload() }, 6000);
    }

    infoSpinner.style.display = spin ? "block" : "none";
    infoContainer.style.display = "block";
    infoTitle.innerHTML = title;
    infoMessage.innerHTML = message;
}

const hideInfoDialog = () => {
    infoContainer.style.display = "none";
}

const TimeLevels = {
    scale: [24, 60, 60, 1],
    units: ['d ', 'h ', 'm ', 's ']
};

const intervalToLevels = (interval, levels) => {
    const cbFun = (d, c) => {
      let bb = d[1] % c[0],
        aa = (d[1] - bb) / c[0];
      aa = aa > 0 ? aa + c[1] : '';

      return [d[0] + aa, bb];
    };

    let rslt = levels.scale.map((d, i, a) => a.slice(i).reduce((d, c) => d * c))
      .map((d, i) => ([d, levels.units[i]]))
      .reduce(cbFun, ['', interval]);
    return rslt[0];
};

const secondsToString = interval => intervalToLevels(interval, TimeLevels);

document.getElementById("ocpp_save_btn").onclick = function() {
    ocppSpinner.className = "spin";
    const data = {
        config: {
            ocpp: {
                url: ocppUrl.value,
                name: ocppName.value,
            },
        },
        save: true,
        reboot: true,
    };
    axios.post(host + "/rpc/Config.Set", data).then(function() {
        showInfoDialog(
            "Device is rebooting and connecting to OCPP backend.",
            "Configuration", true, true);
    }).catch(function(err) {
        console.error(err);
        const msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        ocppSpinner.className = "";
    });
};

document.getElementById("wifi_save_btn").onclick = function() {
    wifiSpinner.className = "spin";
    const data = {
        config: {
            wifi: {
                sta: { enable: true, ssid: wifiSSID.value, pass: wifiPass.value},
                ap: { enable: false },
            },
        },
        save: true,
        reboot: true,
    };
    axios.post(host + "/rpc/Config.Set", data).then(function() {
        const deviceIdStr = document.getElementById("device_id").innerText;
        showInfoDialog(
            "Device is rebooting and connecting to " + wifiSSID.value + ".<br/>" +
            "Connect to the same network and visit <a href='http://" + deviceIdStr + ".local/'>" +
            deviceIdStr + ".local.</a>.",
            "Configuration", false, false);
    }).catch(function(err) {
        console.error(err);
        const msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        wifiSpinner.className = "";
    });
};

document.getElementById("reset_btn").onclick = function() {
    const ok = confirm("This action will wipe all user configuration\n" +
        "and reset the device to factory settings.\nDo you want to proceed?");
    if (!ok) {
        return;
    }
    resetSpinner.className = "spin";
    const data = {};
    axios.post(host + "/rpc/Wallbox.Reset", data).then(function() {
        showInfoDialog(
            "Device configuration is reset.<br>" +
            "Reconnect to Wallbox Hotspot to configure the WiFi.",
            "Reset",
            false,
            false);
    }).catch(function(err) {
        console.error(err);
        const msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        resetSpinner.className = "";
    });
};

document.getElementById("reboot_btn").onclick = function() {
    const ok = confirm("This action will restart the device.\nDo you want to proceed?");
    if (!ok) {
        return;
    }
    rebootSpinner.className = "spin";
    const data = {};
    axios.post(host + "/rpc/Wallbox.Reboot", data).then(function() {
        showInfoDialog(
            "Device is rebooting. Please wait...",
            "Reboot",
            true,
            true);
    }).catch(function(err) {
        console.error(err);
        const msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        rebootSpinner.className = "";
    });
};

const getInfo = () => {
    refreshSpinner.className = "spin";
    axios.get(host + "/rpc/Wallbox.GetInfo").then(function(res) {
        wifiSSID.value = res.data.wifi_ssid;
        ocppUrl.value = res.data.ocpp_url;
        ocppName.value = res.data.ocpp_name;
        document.getElementById("device_id").innerText = res.data.id;
        document.getElementById("device_mac").innerText = res.data.mac;
        document.getElementById("app_name").innerText = res.data.app;
        document.getElementById("app_version").innerText = res.data.version;
        document.getElementById("app_build").innerText = res.data.fw_build;
        document.getElementById("app_date").innerText = res.data.fw_ts;
        document.getElementById("energy").innerText = (res.data.energy ? res.data.energy / 3600 : 0).toFixed(2);
        document.getElementById("power").innerText = (res.data.power ? res.data.power.toFixed(2) : "-");
        document.getElementById("uptime").innerText = (res.data.uptime ? secondsToString(res.data.uptime) : "-");
        const state = res.data.state;
        deviceState.innerText = state ? "Charging" : "Available";
        deviceState.className = state ? "connected" : "";
        state = res.data.ocpp_state;
        ocppState.innerText = state ? "Connected" : "Disconnected";
        ocppState.className = state ? "connected" : "disconnected";
    }).catch(function(err) {
        console.error(err);
        alert(err);
    }).then(function() {
        refreshSpinner.className = "";
    });
}

const refreshInfo = () => {
    refreshSpinner.className = "spin";
    axios.get(host + "/rpc/Wallbox.GetInfo").then(function(res) {
        document.getElementById("energy").innerText = (res.data.energy ? res.data.energy / 3600 : 0).toFixed(2);
        document.getElementById("power").innerText = (res.data.power ? res.data.power.toFixed(2) : "-");
        document.getElementById("uptime").innerText = (res.data.uptime ? secondsToString(res.data.uptime) : "-");
        const state = res.data.state;
        deviceState.innerText = state ? "Charging" : "Available";
        deviceState.className = state ? "connected" : "";
        state = res.data.ocpp_state;
        ocppState.innerText = state ? "Connected" : "Disconnected";
        ocppState.className = state ? "connected" : "disconnected";
    }).catch(function(err) {
        console.error(err);
        alert(err);
    }).then(function() {
        refreshSpinner.className = "";
    });
}

document.getElementById("refresh_btn").onclick = getInfo;

const updateFirmware = evt => {
    evt.preventDefault();
    const selectedFile = document.getElementById("fw_select_file").files[0];

    if (selectedFile) {
        console.info("Updating firmware...");
        firmwareSpinner.className = "spin";
        const formData = new FormData();
        formData.append("file", selectedFile);

        showInfoDialog(
            "Updating firmware, please wait...",
            "Update",
            true,
            false);

        axios.post('/update', formData, {
            headers: {
                'Content-Type': "multipart/form-data; boundary=" + formData._boundary
            }
        }).then(function(response) {
            firmwareSpinner.className = "";
            console.info("Firmware updated");
            console.log(response);
            hideInfoDialog();
            showInfoDialog(
                "Firmware update successful.<br>Device is rebooting, please wait...",
                "Update",
                true,
                true);
        }).catch(function(error) {
            firmwareSpinner.className = "";
            console.error(error);
            hideInfoDialog();
            alert("Update failed. Check firmware file.");
        });
    }
}

document.getElementById("fw_upload_btn").onclick = updateFirmware;

(function(){
    getInfo();
    refreshTimer = setInterval(refreshInfo, 10000);
}());
