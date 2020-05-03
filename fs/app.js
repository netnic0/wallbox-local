var host = "";
var refreshTimer;

var wifiEn = document.getElementById("wifi_en");
var wifiSSID = document.getElementById("wifi_ssid");
var wifiPass = document.getElementById("wifi_pass");
var ocppUrl = document.getElementById("ocpp_url");
var ocppName = document.getElementById("ocpp_name");
var deviceState = document.getElementById("state");
var ocppState = document.getElementById("ocpp_state");

var refreshSpinner = document.getElementById("refresh_spinner");
var wifiSpinner = document.getElementById("wifi_spinner");
var ocppSpinner = document.getElementById("ocpp_spinner");
var rebootSpinner = document.getElementById("reboot_spinner");
var resetSpinner = document.getElementById("reset_spinner");

document.getElementById("fw_upload_form").onsubmit = function() {
    document.getElementById("fw_spinner").className = "spin";
    return true;
};

document.getElementById("ocpp_save_btn").onclick = function() {
    ocppSpinner.className = "spin";
    var data = {
        config: {
        ocpp: {
            url: ocpp_url.value,
            name: ocpp_name.value,
        },
        },
        save: true,
        reboot: true,
    };
    axios.post(host + "/rpc/Config.Set", data).then(function(res) {
        document.body.innerHTML =
        "<div class='container'><h1>Rebooting...</h1>" +
        "<p>Device is rebooting and connecting to OCPP Backend";
    }).catch(function(err) {
        if (err.response) {
        err = err.response.data.message;
        }
        alert(err);
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
    axios.post(host + "/rpc/Config.Set", data).then(function(res) {
        document.body.innerHTML =
        "<div class='container'><h1>Rebooting...</h1>" +
        "<p>Device is rebooting and connecting to " + wifiSSID.value + "." +
        "<p>Connect to the same network and visit " +
        "<a href='http://" + document.getElementById("device_id").innerText + ".local/'>" +
        document.getElementById("device_id").innerText + ".local.</a></div>.";
    }).catch(function(err) {
        if (err.response) {
        err = err.response.data.message;
        }
        alert(err);
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
    axios.post(host + "/rpc/Shelly.Reset", data).then(function(res) {
        document.body.innerHTML =
        "<div class='container'><h1>Resetting...</h1>" +
        "<p>Device configuration is reset. Device is rebooting";
    }).catch(function(err) {
        if (err.response) {
        err = err.response.data.message;
        }
        alert(err);
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
    axios.post(host + "/rpc/Shelly.Reboot", data).then(function(res) {
        document.body.innerHTML =
            "<div class='container'><h1>Rebooting...</h1>" +
            "<div class='centered'><span id='spinner' class='spin reboot'></span> Device is rebooting";
        clearInterval(refreshTimer);
        setTimeout(function(){document.location.reload()}, 6000);
    }).catch(function(err) {
        if (err.response) {
            err = err.response.data.message;
        }
        alert(err);
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
        alert(err);
    }).then(function() {
        refreshSpinner.className = "";
    });
}

document.getElementById("refresh_btn").onclick = getInfo;

(function(){
    getInfo();
    refreshTimer = setInterval(getInfo, 30000);
})();
