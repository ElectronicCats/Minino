/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minino RF Spectrum Web Server & Live Canvas Dashboard
 */

#include "rf_spectrum_web.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "rf_spectrum.h"

static const char* TAG = "rf_spectrum_web";
static httpd_handle_t s_web_handle = NULL;
/* Scratch buffer for the admin header check in spectrum_control_handler(). */
static char s_admin_hdr[16];

static const char s_dashboard_html[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Minino RF Spectrum</title>"
"<style>"
"body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;margin:0;padding:16px;text-align:center}"
"h1{color:#58a6ff;margin-bottom:4px;font-size:22px}"
".card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px;margin:12px auto;max-width:640px}"
"canvas{width:100%;background:#030712;border-radius:6px;border:1px solid #30363d;display:block}"
".metrics{display:flex;justify-content:space-around;font-weight:bold;margin:8px 0;font-size:15px}"
".val{color:#3fb950}"
".btn{background:#238636;color:#fff;border:none;border-radius:6px;padding:8px 14px;margin:4px;cursor:pointer;font-weight:600}"
".btn:hover{background:#2ea043}"
".btn-danger{background:#da3633}.btn-danger:hover{background:#f85149}"
".btn-secondary{background:#21262d;border:1px solid #30363d;color:#c9d1d9}"
".controls{margin-top:10px}"
"input[type=range]{width:80%;accent-color:#58a6ff}"
"</style></head><body>"
"<h1>Minino RF Spectrum Analyzer</h1>"
"<div style='font-size:12px;color:#8b949e'>ESP32-C6 2.4 GHz Multi-Protocol, Fox Hunting & RF IDS</div>"
"<div id='alertBanner' style='display:none;background:#da3633;color:#fff;font-weight:bold;padding:6px;border-radius:6px;margin:8px auto;max-width:640px'></div>"
"<div class='card'>"
"<div class='metrics'>"
"<div>CH: <span id='ch' class='val'>--</span></div>"
"<div>RSSI: <span id='rssi' class='val'>-- dBm</span></div>"
"<div>PEAK: <span id='peak' class='val'>-- dBm</span></div>"
"<div>TH: <span id='th' class='val'>-- dBm</span></div>"
"<div>IDS: <span id='deauth' class='val'>0</span></div>"
"</div>"
"<canvas id='specCanvas' width='600' height='240'></canvas>"
"<div class='controls'>"
"<label>Umbral (Threshold): <span id='thLabel'>-85 dBm</span></label><br>"
"<input type='range' id='thSlider' min='-95' max='-25' step='5' value='-85' oninput='setTh(this.value)'><br>"
"<button class='btn btn-secondary' onclick='toggleAudio()'>🔊 Audio Geiger (<span id='audState'>ON</span>)</button>"
"<button class='btn btn-secondary' onclick='toggleLog()'>💾 SD Log (<span id='sdState'>OFF</span>)</button>"
"<button class='btn btn-secondary' onclick='toggleProto()'>⚡ Banda: <span id='protoState'>Wi-Fi</span></button>"
"<br><button class='btn btn-danger' onclick='bootSDR()'>🚀 Captura I/Q con ESP-SDR</button>"
"</div></div>"
"<script>"
"const canvas=document.getElementById('specCanvas'),ctx=canvas.getContext('2d');"
"let curData={channels:[],th:-85,ch:1};"
"function drawSpectrum(){"
"ctx.fillStyle='#030712';ctx.fillRect(0,0,canvas.width,canvas.height);"
"const chs=curData.channels||[];if(!chs.length)return;"
"const bw=canvas.width/chs.length,baseY=canvas.height-20;"
"ctx.strokeStyle='#30363d';ctx.beginPath();ctx.moveTo(0,baseY);ctx.lineTo(canvas.width,baseY);ctx.stroke();"
"const thY=baseY-((curData.th+95)/70)*(baseY-20);"
"ctx.strokeStyle='#f85149';ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(0,thY);ctx.lineTo(canvas.width,thY);ctx.stroke();ctx.setLineDash([]);"
"chs.forEach((r,i)=>{"
"const h=Math.max(2,((r+95)/70)*(baseY-20)),x=i*bw+4,w=bw-8,y=baseY-h;"
"const grad=ctx.createLinearGradient(0,baseY,0,y);"
"grad.addColorStop(0,'#1f6feb');grad.addColorStop(1,(r>=curData.th)?'#3fb950':'#388bfd');"
"ctx.fillStyle=grad;ctx.fillRect(x,y,w,h);"
"ctx.fillStyle='#8b949e';ctx.font='11px monospace';ctx.textAlign='center';"
"ctx.fillText(i+1,x+w/2,baseY+14);"
"});}"
"function update(){"
"fetch('/api/spectrum').then(r=>r.json()).then(d=>{"
"curData=d;"
"document.getElementById('ch').innerText=d.ch;"
"document.getElementById('rssi').innerText=d.rssi+' dBm';"
"document.getElementById('peak').innerText=d.peak+' dBm';"
"document.getElementById('th').innerText=(d.th<=-95)?'OFF':d.th+' dBm';"
"document.getElementById('thLabel').innerText=(d.th<=-95)?'OFF':d.th+' dBm';"
"document.getElementById('audState').innerText=d.audio?'ON':'MUTE';"
"document.getElementById('sdState').innerText=d.sd?'REC':'OFF';"
"document.getElementById('protoState').innerText=d.proto||'Wi-Fi';"
"document.getElementById('deauth').innerText=d.deauth||0;"
"const ab=document.getElementById('alertBanner');"
"if(d.alert&&d.alert.length>0){ab.innerText=d.alert;ab.style.display='block';}else{ab.style.display='none';}"
"drawSpectrum();"
"}).catch(()=>{});}"
"setInterval(update,250);"
"function ctl(url){return fetch(url,{method:'POST',headers:{'X-Minino-Admin':'1'}});}"
"function setTh(v){ctl('/api/spectrum/control?th='+v);}"
"function toggleAudio(){ctl('/api/spectrum/control?cmd=audio').then(update);}"
"function toggleLog(){ctl('/api/spectrum/control?cmd=log').then(update);}"
"function toggleProto(){ctl('/api/spectrum/control?cmd=proto').then(update);}"
"</script></body></html>";

static esp_err_t spectrum_html_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, s_dashboard_html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t spectrum_api_handler(httpd_req_t* req) {
  /* Wide enough for the header plus one entry per tracked channel (26). */
  char json_buf[768];
  size_t len = rf_spectrum_get_status_json(json_buf, sizeof(json_buf));
  if (len >= sizeof(json_buf)) {
    len = sizeof(json_buf) - 1;  /* truncated, but always NUL terminated */
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buf, len);
  return ESP_OK;
}

/*
 * Gate for /api/spectrum/control.
 *
 * This endpoint can flip the boot partition and restart the board, so it must
 * not be reachable from a page on another origin. A POST with no custom
 * header is a CORS "simple request": no preflight is sent, the browser happily
 * fires it at the device, and the handler used to act on it. Requiring a custom
 * header forces a preflight, which the device does not answer, so the browser
 * blocks cross-origin calls while the dashboard we serve (same origin) still
 * works unchanged.
 */
static bool control_request_is_privileged(httpd_req_t* req) {
  return httpd_req_get_hdr_value_str(req, RF_SPECTRUM_ADMIN_HDR,
                                     s_admin_hdr, sizeof(s_admin_hdr)) ==
         ESP_OK;
}

static esp_err_t spectrum_control_handler(httpd_req_t* req) {
  char query[128];
  if (!control_request_is_privileged(req)) {
    ESP_LOGW(TAG, "rejected spectrum control request without %s",
             RF_SPECTRUM_ADMIN_HDR);
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"forbidden\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char val[32];
    if (httpd_query_key_value(query, "th", val, sizeof(val)) == ESP_OK) {
      int8_t th = (int8_t)atoi(val);
      rf_spectrum_set_threshold(th);
    }
    if (httpd_query_key_value(query, "cmd", val, sizeof(val)) == ESP_OK) {
      if (strcmp(val, "audio") == 0) {
        rf_spectrum_toggle_audio();
      } else if (strcmp(val, "log") == 0) {
        rf_spectrum_toggle_sd_log();
      } else if (strcmp(val, "proto") == 0) {
        rf_protocol_t p = (rf_spectrum_get_protocol() == RF_PROTOCOL_WIFI) ? RF_PROTOCOL_ZIGBEE : RF_PROTOCOL_WIFI;
        rf_spectrum_set_protocol(p);
      }
    }
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t rf_spectrum_web_start(void) {
  if (s_web_handle != NULL) {
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  /* Not 80: the captive portal and the File Manager web browser both bind the
     default port, and Wi-Fi is up whenever this app runs, so httpd_start() on
     80 fails and the dashboard silently disappears. 8080 keeps it out of their
     way. */
  config.server_port = RF_SPECTRUM_WEB_PORT;
  config.ctrl_port = RF_SPECTRUM_WEB_CTRL_PORT;
  config.max_open_sockets = 4;

  esp_err_t err = httpd_start(&s_web_handle, &config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not start spectrum web server on port %d: %s",
             RF_SPECTRUM_WEB_PORT, esp_err_to_name(err));
    return err;
  }

  httpd_uri_t uri_html = {
      .uri = "/spectrum",
      .method = HTTP_GET,
      .handler = spectrum_html_handler,
      .user_ctx = NULL};

  httpd_uri_t uri_api = {
      .uri = "/api/spectrum",
      .method = HTTP_GET,
      .handler = spectrum_api_handler,
      .user_ctx = NULL};

  httpd_uri_t uri_ctrl = {
      .uri = "/api/spectrum/control",
      .method = HTTP_POST,
      .handler = spectrum_control_handler,
      .user_ctx = NULL};

  httpd_register_uri_handler(s_web_handle, &uri_html);
  httpd_register_uri_handler(s_web_handle, &uri_api);
  httpd_register_uri_handler(s_web_handle, &uri_ctrl);

  ESP_LOGI(TAG, "RF Spectrum Web Dashboard active at /spectrum");
  return ESP_OK;
}

void rf_spectrum_web_stop(void) {
  if (s_web_handle != NULL) {
    httpd_stop(s_web_handle);
    s_web_handle = NULL;
    ESP_LOGI(TAG, "RF Spectrum Web Dashboard stopped");
  }
}
