/*
 * http_server.c
 *
 *  Created on: 17-Jul-2023
 *      Author: xpress_embedo
 */
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"

#include "http_server.h"
#include "wifi_ap.h"

#include "OTA.h"

// Macros
#define HTTP_SERVER_MAX_URI_HANDLERS     (20u)
#define HTTP_SERVER_RECEIVE_WAIT_TIMEOUT (10u)  // in seconds
#define HTTP_SERVER_SEND_WAIT_TIMEOUT    (10u)  // in seconds
#define HTTP_SERVER_MONITOR_QUEUE_LEN    (3u)

// Private Variables
static const char TAG[] = "http_server";
// HTTP Server Task Handle
static httpd_handle_t http_server_handle = NULL;
// HTTP Server Monitor Task Handler
static TaskHandle_t task_http_server_monitor = NULL;
// Queue Handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_q_handle;
// Firmware Update Status
static int fw_update_status = OTA_UPDATE_PENDING;
// ESP32 Timer Configuration Passed to esp_timer_create
static const esp_timer_create_args_t fw_update_reset_args = {
    .callback = &http_server_fw_update_reset_cb,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "fw_update_reset"};
esp_timer_handle_t fw_update_reset;

// Embedded Files: JQuery, index.html, app.css, app.js, and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[] asm(
    "_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[] asm(
    "_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

// Private Function Prototypes
static void http_server_monitor(void* pvParameter);
static httpd_handle_t http_server_configure(void);
static esp_err_t http_server_j_query_handler(httpd_req_t* req);
static esp_err_t http_server_index_html_handler(httpd_req_t* req);
static esp_err_t http_server_app_css_handler(httpd_req_t* req);
static esp_err_t http_server_app_js_handler(httpd_req_t* req);
static esp_err_t http_server_favicon_handler(httpd_req_t* req);
static esp_err_t http_server_ota_update_handler(httpd_req_t* req);
static esp_err_t http_server_ota_status_handler(httpd_req_t* req);
static esp_err_t http_server_sensor_value_handler(httpd_req_t* req);
static void http_server_fw_update_reset_timer(void);

// Public Function Definition
/*
 * Starts the HTTP Server
 */
void http_server_start(void) {
  if (http_server_handle == NULL) {
    http_server_handle = http_server_configure();
  }
}

/*
 * Stops the HTTP Server
 */
void http_server_stop(void) {
  if (http_server_handle) {
    httpd_stop(http_server_handle);
    ESP_LOGI(TAG, "http_server_stop: stopping HTTP Server");
    http_server_handle = NULL;
  }

  if (task_http_server_monitor) {
    vTaskDelete(task_http_server_monitor);
    ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
    task_http_server_monitor = NULL;
  }
}

/*
 * Timer Callback function which calls esp_restart function upon successful
 * firmware update
 */
void http_server_fw_update_reset_cb(void* arg) {
  ESP_LOGI(TAG,
           "http_fw_update_reset_cb: Timer timed-out, restarting the device");
  esp_restart();
}

/*
 * Sends a message to the Queue
 * @param msg_id Message ID from the http_server_msg_e enum
 * @return pdTRUE if an item was successfully sent to the queue, otherwise
 * pdFALSE
 */
BaseType_t http_server_monitor_send_msg(http_server_msg_e msg_id) {
  http_server_q_msg_t msg;
  msg.msg_id = msg_id;
  return xQueueSend(http_server_monitor_q_handle, &msg, portMAX_DELAY);
}

// Private Function Definitions
/*
 * HTTP Server Monitor Task used to track events of the HTTP Server.
 * @param pvParameter parameters which can be passed to the task
 * @return http server instance handle if successful, NULL otherwise
 */
static void http_server_monitor(void* pvParameter) {
  http_server_q_msg_t msg;
  for (;;) {
    if (xQueueReceive(http_server_monitor_q_handle, &msg, portMAX_DELAY)) {
      switch (msg.msg_id) {
        case HTTP_MSG_WIFI_CONNECT_INIT:
          ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
          break;
        case HTTP_MSG_WIFI_CONNECT_SUCCESS:
          ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
          break;
        case HTTP_MSG_WIFI_CONNECT_FAIL:
          ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
          break;
        case HTTP_MSG_WIFI_OTA_UPDATE_SUCCESSFUL:
          ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
          fw_update_status = OTA_UPDATE_SUCCESSFUL;
          http_server_fw_update_reset_timer();
          break;
        case HTTP_MSG_WIFI_OTA_UPDATE_FAILED:
          ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
          fw_update_status = OTA_UPDATE_FAILED;
          break;
        default:
          break;
      }
    }
  }
}

/*
 * Sets up the default httpd server configuration
 * @return http server instance handle if successful, NULL otherwise
 */
static httpd_handle_t http_server_configure(void) {
  // Generate the default configuration
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // create HTTP Server Monitor Task
  xTaskCreate(&http_server_monitor, "http_server_monitor", 4 * 1024u, NULL, 3u,
              &task_http_server_monitor);

  // create a message queue
  http_server_monitor_q_handle =
      xQueueCreate(HTTP_SERVER_MONITOR_QUEUE_LEN, sizeof(http_server_q_msg_t));

  // No need to specify the core id as this is esp32s2 with single core

  // Adjust the default priority to 1 less than the wifi application task
  config.task_priority = 4u;

  // Specify the Stack Size (default is 4096)
  config.stack_size = 4 * 1024u;

  // Increase our URI Handlers
  config.max_uri_handlers = HTTP_SERVER_MAX_URI_HANDLERS;

  // Increase the timeout limits
  config.recv_wait_timeout = HTTP_SERVER_RECEIVE_WAIT_TIMEOUT;
  config.send_wait_timeout = HTTP_SERVER_SEND_WAIT_TIMEOUT;

  ESP_LOGI(TAG,
           "http_server_configure: Starting Server on port: '%d' with task "
           "priority: '%d'",
           config.server_port, config.task_priority);

  // Start the httpd server port
  if (httpd_start(&http_server_handle, &config) == ESP_OK) {
    ESP_LOGI(TAG, "http_server_configure: Registering URI Handlers");
    // Register jQuery handler
    httpd_uri_t jquery_js = {.uri = "/jquery-3.3.1.min.js",
                             .method = HTTP_GET,
                             .handler = http_server_j_query_handler,
                             .user_ctx = NULL};
    // Register index.html handler
    httpd_uri_t index_html = {.uri = "/",
                              .method = HTTP_GET,
                              .handler = http_server_index_html_handler,
                              .user_ctx = NULL};
    // Register app.css handler
    httpd_uri_t app_css = {.uri = "/app.css",
                           .method = HTTP_GET,
                           .handler = http_server_app_css_handler,
                           .user_ctx = NULL};
    // Register app.js handler
    httpd_uri_t app_js = {.uri = "/app.js",
                          .method = HTTP_GET,
                          .handler = http_server_app_js_handler,
                          .user_ctx = NULL};
    // Register favicon.ico handler
    httpd_uri_t favicon_ico = {.uri = "/favicon.ico",
                               .method = HTTP_GET,
                               .handler = http_server_favicon_handler,
                               .user_ctx = NULL};
    // Register OTA Update Handler
    httpd_uri_t ota_update = {.uri = "/OTAupdate",
                              .method = HTTP_POST,
                              .handler = http_server_ota_update_handler,
                              .user_ctx = NULL};

    // Register OTA Status Handler
    httpd_uri_t ota_status = {.uri = "/OTAstatus",
                              .method = HTTP_POST,
                              .handler = http_server_ota_status_handler,
                              .user_ctx = NULL};

    // Register Query Handler
    httpd_register_uri_handler(http_server_handle, &jquery_js);
    httpd_register_uri_handler(http_server_handle, &index_html);
    httpd_register_uri_handler(http_server_handle, &app_css);
    httpd_register_uri_handler(http_server_handle, &app_js);
    httpd_register_uri_handler(http_server_handle, &favicon_ico);
    httpd_register_uri_handler(http_server_handle, &ota_update);
    httpd_register_uri_handler(http_server_handle, &ota_status);
    return http_server_handle;
  }

  ESP_LOGI(TAG, "http_server_configure: Error starting server!");
  return NULL;
}

/*
 * jQuery get handler requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_j_query_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "JQuery Requested");
  httpd_resp_set_type(req, "application/javascript");
  error = httpd_resp_send(req, (const char*) jquery_3_3_1_min_js_start,
                          jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG,
             "http_server_j_query_handler: Error %d while sending Response",
             error);
  } else {
    ESP_LOGI(TAG, "http_server_j_query_handler: Response Sent Successfully");
  }
  return error;
}

/*
 * Send the index HTML page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "Index HTML Requested");
  httpd_resp_set_type(req, "text/html");
  error = httpd_resp_send(req, (const char*) index_html_start,
                          index_html_end - index_html_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG,
             "http_server_index_html_handler: Error %d while sending Response",
             error);
  } else {
    ESP_LOGI(TAG, "http_server_index_html_handler: Response Sent Successfully");
  }
  return error;
}

/*
 * app.css get handler is requested when accessing the web page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "APP CSS Requested");
  httpd_resp_set_type(req, "text/css");
  error = httpd_resp_send(req, (const char*) app_css_start,
                          app_css_end - app_css_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG,
             "http_server_app_css_handler: Error %d while sending Response",
             error);
  } else {
    ESP_LOGI(TAG, "http_server_app_css_handler: Response Sent Successfully");
  }
  return error;
}

/*
 * app.js get handler requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "APP JS Requested");
  httpd_resp_set_type(req, "application/javascript");
  error = httpd_resp_send(req, (const char*) app_js_start,
                          app_js_end - app_js_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG, "http_server_app_js_handler: Error %d while sending Response",
             error);
  } else {
    ESP_LOGI(TAG, "http_server_app_js_handler: Response Sent Successfully");
  }
  return error;
}

/*
 * Sends the .ico file when accessing the web page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_handler(httpd_req_t* req) {
  esp_err_t error;
  ESP_LOGI(TAG, "Favicon.ico Requested");
  httpd_resp_set_type(req, "image/x-icon");
  error = httpd_resp_send(req, (const char*) favicon_ico_start,
                          favicon_ico_end - favicon_ico_start);
  if (error != ESP_OK) {
    ESP_LOGI(TAG,
             "http_server_favicon_handler: Error %d while sending Response",
             error);
  } else {
    ESP_LOGI(TAG, "http_server_favicon_handler: Response Sent Successfully");
  }
  return error;
}

/**
 * @brief Receives the *.bin file via the web page and handles the firmware
 * update
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK, other ESP_FAIL if timeout occurs and the update canot be
 * started
 */
static esp_err_t http_server_ota_update_handler(httpd_req_t* req) {
  esp_err_t error;
  esp_ota_handle_t ota_handle;
  char ota_buffer[1024];
  int content_len = req->content_len;  // total content length
  int content_received = 0;
  int recv_len = 0;
  bool is_req_body_started = false;
  bool flash_successful = false;

  is_ota_running = true;
  OTA_call_show_event_cb(OTA_SHOW_START_EVENT, NULL);

  // get the next OTA app partition which should be written with a new firmware
  const esp_partition_t* update_partition =
      esp_ota_get_next_update_partition(NULL);

  // our ota_buffer is not sufficient to receive all data in a one go
  // hence we will read the data in chunks and write in chunks, read the below
  // mentioned comments for more information
  do {
    // The following is the API to read content of data from the HTTP request
    /* This API will read HTTP content data from the HTTP request into the
     * provided buffer. Use content_len provided in the httpd_req_t structure to
     *  know the length of the data to be fetched.
     *  If the content_len is to large for the buffer then the user may have to
     *  make multiple calls to this functions (as done here), each time fetching
     *  buf_len num of bytes (which is ota_buffer length here), while the
     * pointer to content data is incremented internally by the same number This
     * function returns Bytes: Number of bytes read into the buffer successfully
     *  0: Buffer length parameter is zero/connection closed by peer.
     *  HTTPD_SOCK_ERR_INVALID: Invalid Arguments
     *  HTTPD_SOCK_ERR_TIMEOUT: Timeout/Interrupted while calling socket recv()
     *  HTTPD_SOCK_ERR_FAIL: Unrecoverable error while calling socket recv()
     *  Parameters to this function are:
     *  req: The request being responded to
     *  ota_buffer: Pointer to a buffer that the data will be read into
     *  length: length of the buffer which ever is minimum (as we don't want to
     *          read more data which buffer can't handle)
     */
    recv_len =
        httpd_req_recv(req, ota_buffer, MIN(content_len, sizeof(ota_buffer)));
    // if recv_len is less than zero, it means some problem (but if timeout
    // error, then try again)
    if (recv_len < 0) {
      // Check if timeout occur, then we will retry again
      if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "http_server_ota_update_handler: Socket Timeout");
        continue;  // Retry Receiving if Timeout Occurred
      }
      // If there is some other error apart from Timeout, then exit with fail
      ESP_LOGI(TAG, "http_server_ota_update_handler: OTA Other Error, %d",
               recv_len);
      OTA_call_show_event_cb(OTA_SHOW_RESULT_EVENT, &flash_successful);
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "http_server_ota_update_handler: OTA RX: %d of %d",
             content_received, content_len);
    uint8_t ota_progress = (content_received * 100) / content_len;

    OTA_call_show_event_cb(OTA_SHOW_PROGRESS_EVENT, &ota_progress);

    // We are here which means that "recv_len" is positive, now we have to check
    // if this is the first data we are receiving or not, If so, it will have
    // the information in the header that we need
    if (!is_req_body_started) {
      is_req_body_started = true;
      // Now we have to identify from where the binary file content is starting
      // this can be done by actually checking the escape characters i.e.
      // \r\n\r\n Get the location of the *.bin file content (remove the web
      // form data) the strstr will return the pointer to the \r\n\r\n in the
      // ota_buffer and then by adding 4 we reach to the start of the binary
      // content/start
      char* body_start_p = strstr(ota_buffer, "\r\n\r\n") + 4u;
      int body_part_len = recv_len - (body_start_p - ota_buffer);
      ESP_LOGI(TAG, "http_server_ota_update_handler: OTA File Size: %d",
               content_len);
      /*
       * esp_ota_begin function commence an OTA update writing to the specified
       * partition. The specified partition is erased to the specified image
       * size. If the image size is not yet known, OTA_SIZE_UNKNOWN is passed
       * which will cause the entire partition to be erased.
       * On Success this function allocates memory that remains in use until
       * esp_ota_end is called with the return handle.
       */
      error = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
      if (error != ESP_OK) {
        ESP_LOGI(TAG,
                 "http_server_ota_update_handler: Error with OTA Begin, "
                 "Canceling OTA");
        OTA_call_show_event_cb(OTA_SHOW_RESULT_EVENT, &flash_successful);
        return ESP_FAIL;
      } else {
        ESP_LOGI(TAG,
                 "http_server_ota_update_handler: Writing to partition subtype "
                 "%d at offset 0x%lx",
                 update_partition->subtype, update_partition->address);
      }
      /*
       * esp_ota_write function, writes the OTA update to the partition.
       * This function can be called multiple times as data is received during
       * the OTA operation. Data is written sequentially to the partition.
       * Here we are writing the body start for the first time.
       */
      esp_ota_write(ota_handle, body_start_p, body_part_len);
      content_received += body_part_len;
    } else {
      /* Continue to receive data above using httpd_req_recv function, and write
       * using esp_ota_write (below), until all the content is received. */
      esp_ota_write(ota_handle, ota_buffer, recv_len);
      content_received += recv_len;
    }

  } while ((recv_len > 0) && (content_received < content_len));
  // till complete data is received and written or some error is there we will
  // remain in the above mentioned do-while loop

  /* Finish the OTA update and validate newly written app image.
   * After calling esp_ota_end, the handle is no longer valid and memory
   * associated with it is freed (regardless of the results).
   */
  if (esp_ota_end(ota_handle) == ESP_OK) {
    // let's update the partition i.e. configure OTA data for new boot partition
    if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
      const esp_partition_t* boot_partition = esp_ota_get_boot_partition();
      ESP_LOGI(TAG,
               "http_server_ota_update_handler: Next boot partition subtype %d "
               "at offset 0x%lx",
               boot_partition->subtype, boot_partition->address);
      flash_successful = true;
    } else {
      ESP_LOGI(TAG, "http_server_ota_update_handler: Flash Error");
    }
  } else {
    ESP_LOGI(TAG, "http_server_ota_update_handler: esp_ota_end Error");
  }

  // We won't update the global variables throughout the file, so send the
  // message about the status
  if (flash_successful) {
    http_server_monitor_send_msg(HTTP_MSG_WIFI_OTA_UPDATE_SUCCESSFUL);
  } else {
    http_server_monitor_send_msg(HTTP_MSG_WIFI_OTA_UPDATE_FAILED);
  }
  OTA_call_show_event_cb(OTA_SHOW_RESULT_EVENT, &flash_successful);
  return ESP_OK;
}

/*
 * OTA status handler responds with the firmware update status after the OTA
 * update is started and responds with the compile time & date when the page is
 * first requested
 * @param req HTTP request for which the URI needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_ota_status_handler(httpd_req_t* req) {
  char ota_JSON[100];
  ESP_LOGI(TAG, "OTA Status Requested");
  sprintf(ota_JSON,
          "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":"
          "\"%s\"}",
          fw_update_status, __TIME__, __DATE__);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, ota_JSON, strlen(ota_JSON));

  return ESP_OK;
}

/*
 * Check the fw_update_status and creates the fw_update_reset time if the
 * fw_update_status is true
 */
static void http_server_fw_update_reset_timer(void) {
  if (fw_update_status == OTA_UPDATE_SUCCESSFUL) {
    ESP_LOGI(TAG,
             "http_server_fw_update_reset_timer: FW Update successful "
             "starting FW update reset timer");
    // Give the web page a chance to receive an acknowledge back and initialize
    // the timer
    ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
    ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8 * 1000 * 1000));
  } else {
    ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW Update unsuccessful");
  }
}
