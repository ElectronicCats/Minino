// JavaScript
var seconds = null;
var otaTimerVar = null;

// Función para obtener información del archivo seleccionado
function getFileInfo() {
    var fileInput = document.getElementById("selected_file");
    var fileInfo = document.getElementById("file_info");
    if (fileInput.files.length > 0) {
        var fileSize = fileInput.files[0].size;
        var fileName = fileInput.files[0].name;
        fileInfo.innerHTML = "<h4>Selected file: " + fileName + "<br>Size: " + fileSize + " bytes</h4>";
        fileInfo.style.opacity = 1;
    } else {
        fileInfo.innerHTML = "";
        fileInfo.style.opacity = 0;
    }
}

// Función para manejar la actualización de firmware
function updateFirmware() {
    var formData = new FormData();
    var fileInput = document.getElementById("selected_file");

    if (fileInput.files.length > 0) {
        var file = fileInput.files[0];
        formData.set("file", file, file.name);

        var request = new XMLHttpRequest();
        request.upload.addEventListener("progress", updateProgress);
        request.open('POST', "/OTAupdate");
        request.responseType = "blob";
        request.send(formData);

        document.getElementById("ota_update_status").innerHTML = "Uploading " + file.name + ", Firmware Update in Progress...";
        document.getElementById("ota_update_status").style.opacity = 1;
    } else {
        window.alert('Select a file first.');
    }
}

// Función para actualizar el progreso de la carga
function updateProgress(oEvent) {
    if (oEvent.lengthComputable) {
        var percentComplete = Math.round((oEvent.loaded / oEvent.total) * 100);
        document.getElementById("ota_update_status").innerHTML = "Uploading... " + percentComplete + "%";

        // Envía el progreso al servidor
        var xhrProgress = new XMLHttpRequest();
        xhrProgress.open('POST', "/OTAprogress", true); // Ruta del endpoint para el progreso
        xhrProgress.setRequestHeader("Content-Type", "application/json");
        xhrProgress.send(JSON.stringify({ "progress": percentComplete }));
    } else {
        document.getElementById("ota_update_status").innerHTML = "Uploading... Please wait.";
    }
}

// Función para obtener el estado de la actualización
function getUpdateStatus() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', "/OTAstatus", true);
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onload = function() {
        if (xhr.status === 200) {
            var response = JSON.parse(xhr.responseText);
            document.getElementById("latest_firmware").innerHTML = response.compile_date + " - " + response.compile_time;

            if (response.ota_update_status === 1) {
                seconds = 10;
                otaRebootTimer();
            } else if (response.ota_update_status === -1) {
                document.getElementById("ota_update_status").innerHTML = "!!! Upload Error !!!";
            }
        } else {
            console.error('Error fetching latest firmware.');
        }
    };

    xhr.send(JSON.stringify({ "ota_update_status": true }));
}

// Función para el temporizador de reinicio después de la actualización
function otaRebootTimer() {
    document.getElementById("ota_update_status").innerHTML = "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " + seconds;

    if (--seconds === 0) {
        clearTimeout(otaTimerVar);
        window.location.reload();
    } else {
        otaTimerVar = setTimeout(otaRebootTimer, 1000);
    }
}

// Ejecutar la función para obtener el estado de actualización al cargar la página
document.addEventListener("DOMContentLoaded", function() {
    getUpdateStatus();
});
