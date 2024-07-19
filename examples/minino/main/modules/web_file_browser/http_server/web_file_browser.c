#include <dirent.h>
#include "esp_http_server.h"
#include "esp_log.h"

#include "web_file_browser.h"

static const char TAG[] = "web_file_browser";

extern const uint8_t html_header_start[] asm("_binary_header_html_start");
extern const uint8_t html_header_end[] asm("_binary_header_html_end");

static httpd_handle_t http_server_handle = NULL;
static httpd_handle_t web_file_browser_start(void);
static esp_err_t list_files_handler(httpd_req_t* req);
static esp_err_t download_get_handler(httpd_req_t* req);

const char* mount_path = "/sdcard";
bool show_hidden_files = false;

void web_file_browser_init() {
  if (http_server_handle == NULL) {
    http_server_handle = web_file_browser_start();
  }
}

void web_file_browser_stop() {
  if (http_server_handle) {
    httpd_stop(http_server_handle);
    ESP_LOGI(TAG, "HTTP Server Stop");
    http_server_handle = NULL;
  }
}

static httpd_handle_t web_file_browser_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  if (httpd_start(&http_server_handle, &config) == ESP_OK) {
    httpd_uri_t file_server = {.uri = "/",
                               .method = HTTP_GET,
                               .handler = list_files_handler,
                               .user_ctx = NULL};

    httpd_uri_t file_download = {.uri = "/download",
                                 .method = HTTP_GET,
                                 .handler = download_get_handler,
                                 .user_ctx = NULL};

    httpd_register_uri_handler(http_server_handle, &file_server);
    httpd_register_uri_handler(http_server_handle, &file_download);
    return http_server_handle;
  }

  ESP_LOGE(TAG, "Error Starting Server");
  return NULL;
}

esp_err_t list_files_handler(httpd_req_t* req) {
  size_t path_len = 200;
  char* path = (char*) malloc(path_len);
  struct dirent* ent;
  DIR* dir;

  size_t query_len = 300;
  char* query_str = (char*) malloc(query_len);
  if (httpd_req_get_url_query_str(req, query_str, query_len) == ESP_OK) {
    esp_err_t parse_err =
        httpd_query_key_value(query_str, "path", path, path_len);
    if (parse_err != ESP_OK) {
      ESP_LOGE(TAG, "Error Parsing Query: %s", esp_err_to_name(parse_err));
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
  } else {
    strncpy(path, mount_path, 15);
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
        req, "<span class=\"folder\">&#x2934;</span><a href=\"/?path=");
    httpd_resp_sendstr_chunk(req, parent_path);
    httpd_resp_sendstr_chunk(req, "\">..</a><br>");
    free(parent_path);
  }

  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.' && !show_hidden_files) {
      continue;
    }
    size_t filepath_len = 500;
    char* filepath = (char*) malloc(filepath_len);
    snprintf(filepath, filepath_len, "%s/%s", path, ent->d_name);

    if (ent->d_type == DT_DIR) {
      httpd_resp_sendstr_chunk(
          req, "<span class=\"folder\">&#128193;</span> <a href=\"/?path=");
      httpd_resp_sendstr_chunk(req, filepath);
      httpd_resp_sendstr_chunk(req, "\">");
      httpd_resp_sendstr_chunk(req, ent->d_name);
      httpd_resp_sendstr_chunk(req, "/</a><br>");
    } else if (ent->d_type == DT_REG) {
      httpd_resp_sendstr_chunk(
          req,
          "<span class=\"file\">&#x1F4E5;</span> <a href=\"/download?path=");
      httpd_resp_sendstr_chunk(req, filepath);
      httpd_resp_sendstr_chunk(req, "\">");
      httpd_resp_sendstr_chunk(req, ent->d_name);
      httpd_resp_sendstr_chunk(req, "</a><br>");
    }
    free(filepath);
    printf("%s/%s\n", path, ent->d_name);
  }
  free(query_str);
  free(path);

  closedir(dir);
  httpd_resp_send_chunk(req, "</body></html>", strlen("</body></html>"));
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
  do {
    read_bytes = fread(buffer, 1, sizeof(buffer), file);
    if (read_bytes > 0) {
      if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
        free(content_disposition_header);
        fclose(file);
        free(filepath);
        free(buf);
        return ESP_FAIL;
      }
    }
  } while (read_bytes > 0);

  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);

  free(content_disposition_header);
  free(filepath);
  free(buf);
  return ESP_OK;
}
