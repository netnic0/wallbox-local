/*eslint no-alert: "off"*/
/*eslint no-console: "off"*/
var axios;

var host = "";
var refreshTimer;

var wifiSSID = document.getElementById("wifi_ssid");
var wifiPass = document.getElementById("wifi_pass");
var ocppUrl = document.getElementById("ocpp_url");
var ocppName = document.getElementById("ocpp_name");
var deviceState = document.getElementById("state");
var ocppState = document.getElementById("ocpp_state");

var infoContainer = document.getElementById("info_container");

var infoSpinner = document.getElementById("info_spinner");
var refreshSpinner = document.getElementById("refresh_spinner");
var wifiSpinner = document.getElementById("wifi_spinner");
var ocppSpinner = document.getElementById("ocpp_spinner");
var rebootSpinner = document.getElementById("reboot_spinner");
var resetSpinner = document.getElementById("reset_spinner");
var firmwareSpinner = document.getElementById("fw_spinner");

var infoTitle = document.getElementById("info_title");
var infoMessage = document.getElementById("info_message");

function showInfoDialog(message, title, spin, reboot) {
    clearInterval(refreshTimer);
    if (reboot) {
        setTimeout(function(){ document.location.reload() }, 6000);
    }

    infoSpinner.style.display = spin ? "block" : "none";

    infoContainer.style.display = "block";
    infoTitle.innerHTML = title;
    infoMessage.innerHTML = message;
}

function hideInfoDialog() {
    infoContainer.style.display = "none";
}

document.getElementById("ocpp_save_btn").onclick = function() {
    ocppSpinner.className = "spin";
    var data = {
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
        var msg = err;
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
    var data = {
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
        var deviceIdStr = document.getElementById("device_id").innerText;
        showInfoDialog(
            "Device is rebooting and connecting to " + wifiSSID.value + ".<br/>" +
            "Connect to the same network and visit <a href='http://" + deviceIdStr + ".local/'>" +
            deviceIdStr + ".local.</a>.",
            "Configuration", false, false);
    }).catch(function(err) {
        console.error(err);
        var msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        wifiSpinner.className = "";
    });
};

document.getElementById("reset_btn").onclick = function() {
    var ok = confirm("This action will wipe all user configuration\n" +
        "and reset the device to factory settings.\nDo you want to proceed?");
    if (!ok) {
        return;
    }
    resetSpinner.className = "spin";
    var data = {};
    axios.post(host + "/rpc/Shelly.Reset", data).then(function() {
        showInfoDialog(
            "Device configuration is reset.<br>" +
            "Reconnect to Wallbox Hotspot to configure the WiFi.",
            "Reset",
            false,
            false);
    }).catch(function(err) {
        console.error(err);
        var msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        resetSpinner.className = "";
    });
};

document.getElementById("reboot_btn").onclick = function() {
    var ok = confirm("This action will restart the device.\nDo you want to proceed?");
    if (!ok) {
        return;
    }
    rebootSpinner.className = "spin";
    var data = {};
    axios.post(host + "/rpc/Shelly.Reboot", data).then(function() {
        showInfoDialog(
            "Device is rebooting. Please wait...",
            "Reboot",
            true,
            true);
    }).catch(function(err) {
        console.error(err);
        var msg = err;
        if (err.response) {
            msg = err.response.data.message;
        }
        alert(msg);
    }).then(function() {
        rebootSpinner.className = "";
    });
};

function getInfo() {
    refreshSpinner.className = "spin";
    axios.get(host + "/rpc/Shelly.GetInfo").then(function(res) {
        wifiSSID.value = res.data.wifi_ssid;
        ocppUrl.value = res.data.ocpp_url;
        ocppName.value = res.data.ocpp_name;
        document.getElementById("device_id").innerText = res.data.id;
        document.getElementById("app_name").innerText = res.data.app;
        document.getElementById("app_version").innerText = res.data.version;
        document.getElementById("app_build").innerText = res.data.fw_build;
        document.getElementById("energy").innerText = (res.data.energy ? res.data.energy / 3600 : 0).toFixed(2);
        document.getElementById("power").innerText = (res.data.power ? res.data.power.toFixed(2) : "-");
        var state = res.data.state;
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

function refreshInfo() {
    refreshSpinner.className = "spin";
    axios.get(host + "/rpc/Shelly.GetInfo").then(function(res) {
        document.getElementById("energy").innerText = (res.data.energy ? res.data.energy / 3600 : 0).toFixed(2);
        document.getElementById("power").innerText = (res.data.power ? res.data.power.toFixed(2) : "-");
        var state = res.data.state;
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

function updateFirmware(evt) {
    evt.preventDefault();
    var selectedFile = document.getElementById("fw_select_file").files[0];

    if (selectedFile) {
        console.info("Updating firmware...");
        firmwareSpinner.className = "spin";
        var formData = new FormData();
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
