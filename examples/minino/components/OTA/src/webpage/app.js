$(document).ready(function()
{
  getUpdateStatus();
});

// Función para obtener información del archivo seleccionado
function getFileInfo() {
    var fileInput = document.getElementById("selected_file");
    var fileInfo = document.getElementById("file_info");
    if (fileInput.files.length > 0 && fileInput.files[0].name == "minino.bin") {
        var fileSize = fileInput.files[0].size;
        var fileName = fileInput.files[0].name;
        fileInfo.innerHTML = "<h4>Selected file: " + fileName + "<br>Size: " + fileSize + " bytes</h4>";
        fileInfo.style.opacity = 1;
    } else {
        fileInfo.innerHTML = "";
        fileInfo.style.opacity = 1;
        window.alert('Please select a "minino.bin" file');
    }
}

// Función para manejar la actualización de firmware
function updateFirmware() {
    var formData = new FormData();
    var fileInput = document.getElementById("selected_file");

    if (fileInput.files.length > 0 && fileInput.files[0].name == "minino.bin") {
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
        window.alert('Please select a "minino.bin" file');
    }
}

// Función para actualizar el progreso de la carga
function updateProgress(oEvent) {
    if (oEvent.lengthComputable) {
        var percentComplete = Math.round((oEvent.loaded / oEvent.total) * 100);
        document.getElementById("ota_update_status").innerHTML = "Uploading... " + percentComplete + "%";
        getUpdateStatus();
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
            document.getElementById("latest_firmware").innerHTML = "Current Firmware Version: " + response.current_fw_version;

            if (response.ota_update_status === 1) {
                window.alert('Firmware Update Success, rebooting system...');
            } else if (response.ota_update_status === -1) {
                window.alert('Something was wrong, please try again');
            }
        }
    };

    xhr.send(JSON.stringify({ "ota_update_status": true }));
}
