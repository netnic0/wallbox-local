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
var mainContainer = document.getElementById("main_container");
var wifiContainer = document.getElementById("wifi_container");
var ocppContainer = document.getElementById("ocpp_container");
var fwContainer = document.getElementById("fw_container");
var adminContainer = document.getElementById("admin_container");

var infoSpinner = document.getElementById("info_spinner");
var refreshSpinner = document.getElementById("refresh_spinner");
var wifiSpinner = document.getElementById("wifi_spinner");
var ocppSpinner = document.getElementById("ocpp_spinner");
var rebootSpinner = document.getElementById("reboot_spinner");
var resetSpinner = document.getElementById("reset_spinner");

var infoTitle = document.getElementById("info_title");
var infoMessage = document.getElementById("info_message");

function showInfoPopup(reboot, title, message) {
    clearInterval(refreshTimer);
    if (reboot) {
        setTimeout(function(){ document.location.reload() }, 6000);
    }

    infoSpinner.className = reboot ? "spin reboot" : "hidden";

    var clazzes = "container hidden";
    mainContainer.className = clazzes;
    wifiContainer.className = clazzes;
    ocppContainer.className = clazzes;
    fwContainer.className = clazzes;
    adminContainer.className = clazzes;

    infoContainer.className = "container popup";
    infoTitle.innerText = title;
    infoMessage.innerText = message;
}

document.getElementById("fw_upload_form").onsubmit = function() {
    document.getElementById("fw_spinner").className = "spin";
    return true;
};

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
        showInfoPopup(true, "Rebooting...", "Device is rebooting and connecting to OCPP backend.");
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
        showInfoPopup(false, "Rebooting...",
            "Device is rebooting and connecting to " + wifiSSID.value + ".<br>" +
            "Connect to the same network and visit <a href='http://" + deviceIdStr + ".local/'>" +
            deviceIdStr + ".local.</a>.");
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
        showInfoPopup(false, "Reset",
            "Device configuration is reset.<br>" +
            "Reconnect to Wallbox Hotspot to configure the WiFi.");
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
        showInfoPopup(true, "Reboot", "Device is rebooting. Please wait...");
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

(function(){
    getInfo();
    refreshTimer = setInterval(refreshInfo, 10000);
}());
