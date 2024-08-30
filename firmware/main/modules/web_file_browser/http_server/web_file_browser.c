#include "web_file_browser.h"

#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"

#include "esp_http_server.h"
#include "files_ops.h"
#include "flash_fs.h"
#include "sd_card.h"

#define WFB_SD_CARD_PATH   "/sdcard"
#define WFB_FLASH_FS_PATH  "/internal"
#define WFB_MOUNT_PATH_LEN 15

static const char TAG[] = "web_file_browser";

extern const uint8_t html_header_start[] asm("_binary_header_html_start");
extern const uint8_t html_header_end[] asm("_binary_header_html_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

static httpd_handle_t http_server_handle = NULL;
static httpd_handle_t web_file_browser_start(void);
static esp_err_t favicon_handler(httpd_req_t* req);
static esp_err_t list_root_paths_handler(httpd_req_t* req);
static esp_err_t list_files_handler(httpd_req_t* req);
static esp_err_t download_get_handler(httpd_req_t* req);
static esp_err_t style_css_handler(httpd_req_t* req);

const char* mount_path = NULL;
bool show_hidden_files = false;

static void web_file_browser_show_event(uint8_t event, void* context) {
  if (wfb_show_event_cb != NULL) {
    wfb_show_event_cb(event, context);
  }
}

void web_file_browser_begin() {
#if !defined(CONFIG_WEB_FILE_BROWSER_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  if (http_server_handle == NULL) {
    http_server_handle = web_file_browser_start();
    mount_path = (char*) malloc(WFB_MOUNT_PATH_LEN);
  } else {
    web_file_browser_show_event(WEB_FILE_BROWSER_ALREADY_EV, NULL);
  }
}

void web_file_browser_stop() {
  if (http_server_handle) {
    httpd_stop(http_server_handle);
    http_server_handle = NULL;
  }
  web_file_browser_show_event(WEB_FILE_BROWSER_STOP_EV, NULL);
}

void web_file_browser_set_show_event_cb(web_file_browser_show_event_cb_t cb) {
  wfb_show_event_cb = cb;
}

static httpd_handle_t web_file_browser_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  if (httpd_start(&http_server_handle, &config) == ESP_OK) {
    httpd_uri_t root_server = {.uri = "/",
                               .method = HTTP_GET,
                               .handler = list_root_paths_handler,
                               .user_ctx = NULL};
    httpd_uri_t file_server = {.uri = "/list",
                               .method = HTTP_GET,
                               .handler = list_files_handler,
                               .user_ctx = NULL};

    httpd_uri_t file_download = {.uri = "/download",
                                 .method = HTTP_GET,
                                 .handler = download_get_handler,
                                 .user_ctx = NULL};
    httpd_uri_t favicon_ico = {.uri = "/favicon_b.ico",
                               .method = HTTP_GET,
                               .handler = favicon_handler,
                               .user_ctx = NULL};
    httpd_uri_t style_css = {.uri = "/style.css",
                             .method = HTTP_GET,
                             .handler = style_css_handler,
                             .user_ctx = NULL};

    httpd_register_uri_handler(http_server_handle, &root_server);
    httpd_register_uri_handler(http_server_handle, &file_server);
    httpd_register_uri_handler(http_server_handle, &file_download);
    httpd_register_uri_handler(http_server_handle, &favicon_ico);
    httpd_register_uri_handler(http_server_handle, &style_css);
    web_file_browser_show_event(WEB_FILE_BROWSER_READY_EV, NULL);
    return http_server_handle;
  }

  web_file_browser_show_event(WEB_FILE_BROWSER_ERROR_EV, NULL);
  return NULL;
}
esp_err_t list_root_paths_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send_chunk(req, (const char*) html_header_start,
                        html_header_end - html_header_start);
  if (flash_fs_mount() == ESP_OK) {
    httpd_resp_sendstr_chunk(
        req, "<span class=\"folder\">&#128193;</span> <a href=\"/list?root=");
    httpd_resp_sendstr_chunk(req, WFB_FLASH_FS_PATH);
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, "Internal");
    httpd_resp_sendstr_chunk(req, "/</a><br>");
  }
  esp_err_t err = sd_card_mount();
  if (err == ESP_OK) {
    httpd_resp_sendstr_chunk(
        req, "<span class=\"folder\">&#128193;</span> <a href=\"/list?root=");
    httpd_resp_sendstr_chunk(req, WFB_SD_CARD_PATH);
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, "SD card");
    httpd_resp_sendstr_chunk(req, "/</a><br>");
  } else {
    printf("SD ERR: %s\n", esp_err_to_name(err));
  }
  httpd_resp_send_chunk(req, "</div></div></body></html>",
                        strlen("</div></div></body></html>"));
  httpd_resp_send_chunk(req, "", 0);
  return ESP_OK;
}

static void send_dirs(DIR* dir, char* path, httpd_req_t* req) {
  struct dirent* ent;
  rewinddir(dir);
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.' && !show_hidden_files) {
      continue;
    }
    size_t filepath_len = strlen(path) + strlen(ent->d_name) + 2;
    ;
    char* filepath = (char*) malloc(filepath_len);
    snprintf(filepath, filepath_len, "%s/%s", path, ent->d_name);

    char* size_str = files_ops_format_size(files_ops_get_file_size_2(filepath));
    if (ent->d_type == DT_DIR) {
      httpd_resp_sendstr_chunk(
          req, "<span class=\"folder\">&#128193;</span> <a href=\"/list?path=");
      httpd_resp_sendstr_chunk(req, filepath);
      httpd_resp_sendstr_chunk(req, "\">");
      httpd_resp_sendstr_chunk(req, ent->d_name);
      httpd_resp_sendstr_chunk(req, "/</a><br>");
    }
    free(filepath);
    free(size_str);
    printf("%s/%s\n", path, ent->d_name);
  }
}
static void send_files(DIR* dir, char* path, httpd_req_t* req) {
  struct dirent* ent;
  rewinddir(dir);
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.' && !show_hidden_files) {
      continue;
    }
    size_t filepath_len = strlen(path) + strlen(ent->d_name) + 2;
    ;
    char* filepath = (char*) malloc(filepath_len);
    snprintf(filepath, filepath_len, "%s/%s", path, ent->d_name);

    char* size_str = files_ops_format_size(files_ops_get_file_size_2(filepath));
    if (ent->d_type == DT_REG) {
      httpd_resp_sendstr_chunk(
          req,
          "<span class=\"file\">&#x1F4E5;</span> <a href=\"/download?path=");
      httpd_resp_sendstr_chunk(req, filepath);
      httpd_resp_sendstr_chunk(req, "\">");
      httpd_resp_sendstr_chunk(req, ent->d_name);
      httpd_resp_sendstr_chunk(req, "</a> (");
      httpd_resp_sendstr_chunk(req, size_str);
      httpd_resp_sendstr_chunk(req, ")<br>");
    }
    free(filepath);
    free(size_str);
    printf("%s/%s\n", path, ent->d_name);
  }
}

esp_err_t list_files_handler(httpd_req_t* req) {
  size_t path_len = 200;
  char* path = (char*) malloc(path_len);
  DIR* dir;

  size_t query_len = 300;
  char* query_str = (char*) malloc(query_len);
  if (httpd_req_get_url_query_str(req, query_str, query_len) == ESP_OK) {
    if (httpd_query_key_value(query_str, "root", mount_path,
                              WFB_MOUNT_PATH_LEN) == ESP_OK) {
      strncpy(path, mount_path, WFB_MOUNT_PATH_LEN);
    } else if (httpd_query_key_value(query_str, "path", path, path_len) !=
               ESP_OK) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
  } else {
    strncpy(path, mount_path, WFB_MOUNT_PATH_LEN);
  }
  dir = opendir(path);
  if (dir == NULL) {
    ESP_LOGE(TAG, "Error Opening Directory %s", path);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send_chunk(req, (const char*) html_header_start,
                        html_header_end - html_header_start);

  if (strcmp(path, mount_path) != 0) {
    size_t parent_path_len = 200;
    char* parent_path = (char*) malloc(parent_path_len);
    char* last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
      strncpy(parent_path, path, last_slash - path);
      parent_path[last_slash - path] = '\0';
    } else {
      strcpy(parent_path, mount_path);
    }

    httpd_resp_sendstr_chunk(
        req, "<span class=\"folder\">&#x2934;</span><a href=\"/list?path=");
    httpd_resp_sendstr_chunk(req, parent_path);
    httpd_resp_sendstr_chunk(req, "\">..</a><br>");
    free(parent_path);
  } else {
    httpd_resp_sendstr_chunk(
        req, "<span class=\"folder\">&#x2934;</span><a href=\"/");
    httpd_resp_sendstr_chunk(req, "\">..</a><br>");
  }

  send_dirs(dir, path, req);
  send_files(dir, path, req);

  free(query_str);
  free(path);

  closedir(dir);
  httpd_resp_send_chunk(req, "</div></div></body></html>",
                        strlen("</div></div></body></html>"));
  httpd_resp_send_chunk(req, "", 0);
  printf("----------------------------------------\n");
  printf("----------------------------------------\n");

  return ESP_OK;
}

static esp_err_t download_get_handler(httpd_req_t* req) {
  size_t buf_len = 300;
  char* buf = (char*) malloc(buf_len);

  esp_err_t err = httpd_req_get_url_query_str(req, buf, buf_len);
  if (err != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed To Get Query:");
    free(buf);
    return ESP_FAIL;
  }

  size_t filepath_len = 200;
  char* filepath = (char*) malloc(filepath_len);
  err = httpd_query_key_value(buf, "path", filepath, filepath_len);
  if (err != ESP_OK) {
    printf("ERR: %s\n", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Parameters");
    free(filepath);
    free(buf);
    return ESP_FAIL;
  }

  FILE* file = fopen(filepath, "rb");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed To Open File: %s", filepath);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File Not Found");
    free(filepath);
    free(buf);
    return ESP_FAIL;
  }

  char* file_name = strrchr(filepath, '/');
  if (file_name == NULL) {
    file_name = filepath;
  } else {
    file_name++;
  }
  char* content_disposition_header = (char*) malloc(50);
  snprintf(content_disposition_header, 50, "attachment; filename=%s",
           file_name);

  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Disposition", content_disposition_header);

  char buffer[1024];
  size_t read_bytes;

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  size_t loaded_size = 0;
  rewind(file);
  web_file_browser_show_event(WEB_FILE_BROWSER_TRANSFER_INIT_EV, file_name);
  do {
    read_bytes = fread(buffer, 1, sizeof(buffer), file);
    if (read_bytes > 0) {
      if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
        web_file_browser_show_event(WEB_FILE_BROWSER_TRANSFERING_FILE_RESULT_EV,
                                    false);
        free(content_disposition_header);
        fclose(file);
        free(filepath);
        free(buf);
        return ESP_FAIL;
      }
      loaded_size += read_bytes;
      uint8_t state = (loaded_size * 100) / file_size;
      web_file_browser_show_event(WEB_FILE_BROWSER_TRANSFER_STATE_EV, &state);
    }
  } while (read_bytes > 0);

  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);

  free(content_disposition_header);
  free(filepath);
  free(buf);
  web_file_browser_show_event(WEB_FILE_BROWSER_TRANSFERING_FILE_RESULT_EV,
                              true);
  return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "Favicon.ico Requested");
  httpd_resp_set_type(req, "image/x-icon");
  error = httpd_resp_send(req, (const char*) favicon_ico_start,
                          favicon_ico_end - favicon_ico_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG, "favicon_handler: Error %d while sending Response", error);
  } else {
    ESP_LOGI(TAG, "favicon_handler: Response Sent Successfully");
  }
  return error;
}

static esp_err_t style_css_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "CSS Requested");
  httpd_resp_set_type(req, "text/css");
  error = httpd_resp_send(req, (const char*) style_css_start,
                          style_css_end - style_css_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG, "style_css_handler: Error %d while sending Response", error);
  } else {
    ESP_LOGI(TAG, "style_css_handler: Response Sent Successfully");
  }
  return error;
}