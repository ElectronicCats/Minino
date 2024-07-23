/*
 * http_server.h
 *
 *  Created on: 17-Jul-2023
 *      Author: xpress_embedo
 */

#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_

// Macros
/* Check the getUpdateStatus in JavaScrip file, the value should match with them
 */
#define OTA_UPDATE_PENDING    (0)
#define OTA_UPDATE_SUCCESSFUL (1)
#define OTA_UPDATE_FAILED     (-1)

/*
 * Messages for the HTTP Monitor
 */
typedef enum http_server_msg {
  HTTP_MSG_WIFI_CONNECT_INIT = 0,
  HTTP_MSG_WIFI_CONNECT_SUCCESS,
  HTTP_MSG_WIFI_CONNECT_FAIL,
  HTTP_MSG_WIFI_OTA_UPDATE_SUCCESSFUL,
  HTTP_MSG_WIFI_OTA_UPDATE_FAILED,
} http_server_msg_e;

/*
 * Structure for Message Queue
 */
typedef struct http_server_q_msg {
  http_server_msg_e msg_id;
} http_server_q_msg_t;

// Public Function Declaration
BaseType_t http_server_monitor_send_msg(http_server_msg_e msg_id);
void http_server_start(void);
void http_server_stop(void);
void http_server_fw_update_reset_cb(void* arg);

#endif /* MAIN_HTTP_SERVER_H_ */
