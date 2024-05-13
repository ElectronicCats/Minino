/* cmd_pcap example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdlib.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#ifdef CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  #include "freertos/timers.h"
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include "cmd_pcap.h"
#include "cmd_sniffer.h"
#include "esp_app_trace.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char* TAG = "cmd_pcap";

#define PCAP_FILE_NAME_MAX_LEN              CONFIG_SNIFFER_PCAP_FILE_NAME_MAX_LEN
#define PCAP_MEMORY_BUFFER_SIZE             CONFIG_SNIFFER_PCAP_MEMORY_SIZE
#define SNIFFER_PROCESS_APPTRACE_TIMEOUT_US (100)
#define SNIFFER_APPTRACE_RETRY              (10)
#define TRACE_TIMER_FLUSH_INT_MS            (1000)

#if CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
/**
 * @brief Pcap memory buffer Type Definition
 *
 */
typedef struct {
  char* buffer;
  uint32_t buffer_size;
} pcap_memory_buffer_t;
#endif

typedef struct {
  bool is_opened;
  bool is_writing;
  bool link_type_set;
#if CONFIG_SNIFFER_PCAP_DESTINATION_SD
  char filename[PCAP_FILE_NAME_MAX_LEN];
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_SD
  pcap_file_handle_t pcap_handle;
  pcap_link_type_t link_type;
#if CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
  pcap_memory_buffer_t pcap_buffer;
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
#ifdef CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  TimerHandle_t trace_flush_timer; /*!< Timer handle for Trace buffer flush */
#endif                             // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
} pcap_cmd_runtime_t;

static pcap_cmd_runtime_t pcap_cmd_rt = {0};

/**
 * @brief Pcap File Header
 *
 */
typedef struct {
  uint32_t magic;     /*!< Magic Number */
  uint16_t major;     /*!< Major Version */
  uint16_t minor;     /*!< Minor Version */
  uint32_t zone;      /*!< Time Zone Offset */
  uint32_t sigfigs;   /*!< Timestamp Accuracy */
  uint32_t snaplen;   /*!< Max Length to Capture */
  uint32_t link_type; /*!< Link Layer Type */
} pcap_file_header_t;

/**
 * @brief Pcap Packet Header
 *
 */
typedef struct {
  uint32_t
      seconds; /*!< Number of seconds since January 1st, 1970, 00:00:00 GMT */
  uint32_t microseconds;   /*!< Number of microseconds when the packet was
                              captured (offset from seconds) */
  uint32_t capture_length; /*!< Number of bytes of captured data, no longer than
                              packet_length */
  uint32_t packet_length;  /*!< Actual length of current packet */
} pcap_packet_header_t;

/**
 * @brief Pcap Runtime Handle
 *
 */
struct pcap_file_t {
  FILE* file;                 /*!< File handle */
  pcap_link_type_t link_type; /*!< Pcap Link Type */
  unsigned int major_version; /*!< Pcap version: major */
  unsigned int minor_version; /*!< Pcap version: minor */
  unsigned int time_zone;     /*!< Pcap timezone code */
  uint32_t endian_magic;      /*!< Magic value related to endian format */
};

#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
static int trace_writefun(void* cookie, const char* buf, int len) {
  return esp_apptrace_write(ESP_APPTRACE_DEST_TRAX, buf, len,
                            SNIFFER_PROCESS_APPTRACE_TIMEOUT_US) == ESP_OK
             ? len
             : -1;
}

static int trace_closefun(void* cookie) {
  return esp_apptrace_flush(ESP_APPTRACE_DEST_TRAX,
                            ESP_APPTRACE_TMO_INFINITE) == ESP_OK
             ? 0
             : -1;
}

void pcap_flush_apptrace_timer_cb(TimerHandle_t pxTimer) {
  esp_apptrace_flush(ESP_APPTRACE_DEST_TRAX, pdMS_TO_TICKS(10));
}
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG

static esp_err_t pcap_close(pcap_cmd_runtime_t* pcap) {
  esp_err_t ret = ESP_OK;
  ESP_GOTO_ON_FALSE(pcap->is_opened, ESP_ERR_INVALID_STATE, err, TAG,
                    ".pcap file is already closed");
  ESP_GOTO_ON_ERROR(pcap_del_session(pcap->pcap_handle) != ESP_OK, err, TAG,
                    "stop pcap session failed");
  pcap->is_opened = false;
  pcap->link_type_set = false;
  pcap->pcap_handle = NULL;
#if CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
  free(pcap->pcap_buffer.buffer);
  pcap->pcap_buffer.buffer_size = 0;
  ESP_LOGI(TAG, "free memory successfully");
#endif
#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  if (pcap->trace_flush_timer != NULL) {
    xTimerDelete(pcap->trace_flush_timer, pdMS_TO_TICKS(100));
    pcap->trace_flush_timer = NULL;
  }
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
err:
  return ret;
}

static esp_err_t pcap_open(pcap_cmd_runtime_t* pcap) {
  esp_err_t ret = ESP_OK;
  /* Create file to write, binary format */
  FILE* fp = NULL;
#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  fp = funopen("trace", NULL, trace_writefun, NULL, trace_closefun);
#elif CONFIG_SNIFFER_PCAP_DESTINATION_SD
  fp = fopen(pcap->filename, "wb+");
#elif CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
  pcap->pcap_buffer.buffer = calloc(PCAP_MEMORY_BUFFER_SIZE, sizeof(char));
  ESP_GOTO_ON_FALSE(pcap->pcap_buffer.buffer, ESP_ERR_NO_MEM, err, TAG,
                    "pcap buffer calloc failed");
  fp = fmemopen(pcap->pcap_buffer.buffer, PCAP_MEMORY_BUFFER_SIZE, "wb+");
#else
  #error "pcap file destination hasn't specified"
#endif
  ESP_GOTO_ON_FALSE(fp, ESP_FAIL, err, TAG, "open file failed");
  pcap_config_t pcap_config = {
      .fp = fp,
      .major_version = PCAP_DEFAULT_VERSION_MAJOR,
      .minor_version = PCAP_DEFAULT_VERSION_MINOR,
      .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
  };
  ESP_GOTO_ON_ERROR(pcap_new_session(&pcap_config, &pcap_cmd_rt.pcap_handle),
                    err, TAG, "pcap init failed");
  pcap->is_opened = true;
  ESP_LOGI(TAG, "open file successfully");
  return ret;
err:
  if (fp) {
    fclose(fp);
  }
  return ret;
}

esp_err_t packet_capture(void* payload,
                         uint32_t length,
                         uint32_t seconds,
                         uint32_t microseconds) {
  return pcap_capture_packet(pcap_cmd_rt.pcap_handle, payload, length, seconds,
                             microseconds);
}

esp_err_t sniff_packet_start(pcap_link_type_t link_type) {
  esp_err_t ret = ESP_OK;
#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  uint32_t retry = 0;
  /* wait until apptrace communication established or timeout */
  while (!esp_apptrace_host_is_connected(ESP_APPTRACE_DEST_TRAX) &&
         (retry < SNIFFER_APPTRACE_RETRY)) {
    retry++;
    ESP_LOGW(TAG, "waiting for apptrace established");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_GOTO_ON_FALSE(retry < SNIFFER_APPTRACE_RETRY, ESP_ERR_TIMEOUT, err, TAG,
                    "waiting for apptrace established timeout");

  pcap_open(&pcap_cmd_rt);
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG

  ESP_GOTO_ON_FALSE(pcap_cmd_rt.is_opened, ESP_ERR_INVALID_STATE, err, TAG,
                    "no .pcap file stream is open");
  if (pcap_cmd_rt.link_type_set) {
    ESP_GOTO_ON_FALSE(link_type == pcap_cmd_rt.link_type, ESP_ERR_INVALID_STATE,
                      err, TAG, "link type error");
    ESP_GOTO_ON_FALSE(!pcap_cmd_rt.is_writing, ESP_ERR_INVALID_STATE, err, TAG,
                      "still sniffing");
  } else {
    pcap_cmd_rt.link_type = link_type;
    /* Create file to write, binary format */
#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
    /* Ethernet Link layer traffic amount may be much less than on Wi-Fi (no
    link management msgs.) and trace data is sent to listener only after filling
    trace buffer. Hence the trace buffer might not be filled prior listener's
    timeout. This condition is resolved by flushing the trace buffer
    periodically. */
    if (link_type == PCAP_LINK_TYPE_ETHERNET) {
      int timer_id = 0xFEED;
      pcap_cmd_rt.trace_flush_timer = xTimerCreate(
          "flush_apptrace_timer", pdMS_TO_TICKS(TRACE_TIMER_FLUSH_INT_MS),
          pdTRUE, (void*) timer_id, pcap_flush_apptrace_timer_cb);
      ESP_GOTO_ON_FALSE(pcap_cmd_rt.trace_flush_timer, ESP_FAIL, err, TAG,
                        "pcap xTimerCreate failed");
      ESP_GOTO_ON_FALSE(xTimerStart(pcap_cmd_rt.trace_flush_timer, 0), ESP_FAIL,
                        err_timer_start, TAG, "pcap xTimerStart failed");
    }
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
    pcap_write_header(pcap_cmd_rt.pcap_handle, link_type);
    pcap_cmd_rt.link_type_set = true;
  }
  pcap_cmd_rt.is_writing = true;
  return ret;

#ifdef CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
err_timer_start:
  xTimerDelete(pcap_cmd_rt.trace_flush_timer, pdMS_TO_TICKS(100));
  pcap_cmd_rt.trace_flush_timer = NULL;
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
err:
  return ret;
}

esp_err_t sniff_packet_stop(void) {
  pcap_cmd_rt.is_writing = false;
#if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  pcap_close(&pcap_cmd_rt);
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  return ESP_OK;
}

#if !CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
static struct {
  struct arg_str* file;
  struct arg_lit* open;
  struct arg_lit* close;
  struct arg_lit* summary;
  struct arg_end* end;
} pcap_args;

long pcap_cmd_get_file_size(FILE* file) {
  fseek(file, 0L, SEEK_END);
  long size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  return size;
}

esp_err_t pcap_cmd_print_summary(pcap_file_handle_t pcap, FILE* print_file) {
  esp_err_t ret = ESP_OK;
  long size = pcap_cmd_get_file_size(pcap->file);
  char* packet_payload = NULL;
  ESP_RETURN_ON_FALSE(pcap && print_file, ESP_ERR_INVALID_ARG, TAG,
                      "invalid argument");
  // file empty is allowed, so return ESP_OK
  ESP_RETURN_ON_FALSE(size, ESP_OK, TAG, "pcap file is empty");
  // packet index (by bytes)
  uint32_t index = 0;
  pcap_file_header_t file_header;
  size_t real_read =
      fread(&file_header, sizeof(pcap_file_header_t), 1, pcap->file);
  ESP_RETURN_ON_FALSE(real_read == 1, ESP_FAIL, TAG,
                      "read pcap file header failed");
  index += sizeof(pcap_file_header_t);
  // print pcap header information
  fprintf(print_file,
          "--------------------------------------------------------------------"
          "----\n");
  fprintf(print_file, "Pcap packet Head:\n");
  fprintf(print_file,
          "--------------------------------------------------------------------"
          "----\n");
  fprintf(print_file, "Magic Number: %" PRIx32 "\n", file_header.magic);
  fprintf(print_file, "Major Version: %d\n", file_header.major);
  fprintf(print_file, "Minor Version: %d\n", file_header.minor);
  fprintf(print_file, "SnapLen: %" PRIu32 "\n", file_header.snaplen);
  fprintf(print_file, "LinkType: %" PRIu32 "\n", file_header.link_type);
  fprintf(print_file,
          "--------------------------------------------------------------------"
          "----\n");
  uint32_t packet_num = 0;
  pcap_packet_header_t packet_header;
  while (index < size) {
    real_read =
        fread(&packet_header, sizeof(pcap_packet_header_t), 1, pcap->file);
    ESP_GOTO_ON_FALSE(real_read == 1, ESP_FAIL, err, TAG,
                      "read pcap packet header failed");
    // print packet header information
    fprintf(print_file, "Packet %" PRIu32 ":\n", packet_num);
    fprintf(print_file, "Timestamp (Seconds): %" PRIu32 "\n",
            packet_header.seconds);
    fprintf(print_file, "Timestamp (Microseconds): %" PRIu32 "\n",
            packet_header.microseconds);
    fprintf(print_file, "Capture Length: %" PRIu32 "\n",
            packet_header.capture_length);
    fprintf(print_file, "Packet Length: %" PRIu32 "\n",
            packet_header.packet_length);
    size_t payload_length = packet_header.capture_length;
    packet_payload = malloc(payload_length);
    ESP_GOTO_ON_FALSE(packet_payload, ESP_ERR_NO_MEM, err, TAG,
                      "no mem to save packet payload");
    real_read = fread(packet_payload, payload_length, 1, pcap->file);
    ESP_GOTO_ON_FALSE(real_read == 1, ESP_FAIL, err, TAG, "read payload error");
    // print packet information
    if (file_header.link_type == PCAP_LINK_TYPE_802_11) {
      // Frame Control Field is coded as LSB first
      fprintf(print_file, "Frame Type: %2x\n", (packet_payload[0] >> 2) & 0x03);
      fprintf(print_file, "Frame Subtype: %2x\n",
              (packet_payload[0] >> 4) & 0x0F);
      fprintf(print_file, "Destination: ");
      for (int j = 0; j < 5; j++) {
        fprintf(print_file, "%2x ", packet_payload[4 + j]);
      }
      fprintf(print_file, "%2x\n", packet_payload[9]);
      fprintf(print_file, "Source: ");
      for (int j = 0; j < 5; j++) {
        fprintf(print_file, "%2x ", packet_payload[10 + j]);
      }
      fprintf(print_file, "%2x\n", packet_payload[15]);
      // Check if the frame is a Beacon frame or Probe Response frame
      uint8_t frame_type = (packet_payload[0] >> 2) & 0x03;
      uint8_t frame_subtype = (packet_payload[0] >> 4) & 0x0F;
      if ((frame_type == 0 && frame_subtype == 8) ||  // Beacon frame
          (frame_type == 0 && frame_subtype == 5)) {  // Probe Response frame
        // The BSSID is located in the Address 3 field
        fprintf(print_file, "BSSID: ");
        for (int j = 0; j < 5; j++) {
          fprintf(print_file, "%2x ", packet_payload[16 + j]);
        }
        fprintf(print_file, "%2x\n", packet_payload[21]);
        // The SSID parameter set is located after the fixed parameters (36
        // bytes)
        uint8_t ssid_length = packet_payload[37];
        fprintf(print_file, "SSID: ");
        for (int j = 0; j < ssid_length; j++) {
          fprintf(print_file, "%c", packet_payload[38 + j]);
        }
        fprintf(print_file, "\n");
        // The DS Parameter Set, which contains the channel, is located after
        // the SSID
        uint8_t supported_rates_length = packet_payload[38 + ssid_length + 1];
        fprintf(print_file, "Channel: %d\n",
                packet_payload[38 + ssid_length + supported_rates_length + 4]);
      }
      fprintf(print_file,
              "----------------------------------------------------------------"
              "--------\n");
    } else {
      fprintf(print_file, "Unknown link type:%" PRIu32 "\n",
              file_header.link_type);
      fprintf(print_file,
              "----------------------------------------------------------------"
              "--------\n");
    }
    free(packet_payload);
    packet_payload = NULL;
    index += packet_header.capture_length + sizeof(pcap_packet_header_t);
    packet_num++;
  }
  fprintf(print_file, "Pcap packet Number: %" PRIu32 "\n", packet_num);
  fprintf(print_file,
          "--------------------------------------------------------------------"
          "----\n");
  return ret;
err:
  if (packet_payload) {
    free(packet_payload);
  }
  return ret;
}

int do_pcap_cmd(int argc, char** argv) {
  int ret = 0;
  int nerrors = arg_parse(argc, argv, (void**) &pcap_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, pcap_args.end, argv[0]);
    return 1;
  }

  /* Check whether or not to close pcap file: "--close" option */
  if (pcap_args.close->count) {
    /* close the pcap file */
    ESP_GOTO_ON_FALSE(!(pcap_cmd_rt.is_writing), ESP_FAIL, err, TAG,
                      "still sniffing, file will not close");
    pcap_close(&pcap_cmd_rt);
    ESP_LOGI(TAG, ".pcap file close done");
    return ret;
  }

  #if CONFIG_SNIFFER_PCAP_DESTINATION_SD
  /* set pcap file name: "-f" option */
  int len =
      snprintf(pcap_cmd_rt.filename, sizeof(pcap_cmd_rt.filename), "%s/%s.pcap",
               CONFIG_SNIFFER_MOUNT_POINT, pcap_args.file->sval[0]);
  if (len >= sizeof(pcap_cmd_rt.filename)) {
    ESP_LOGW(TAG,
             "pcap file name too long, try to enlarge memory in menuconfig");
  }

  /* Check if needs to be parsed and shown: "--summary" option */
  if (pcap_args.summary->count) {
    ESP_LOGI(TAG, "%s is to be parsed", pcap_cmd_rt.filename);
    if (pcap_cmd_rt.is_opened) {
      ESP_GOTO_ON_FALSE(!(pcap_cmd_rt.is_writing), ESP_FAIL, err, TAG,
                        "still writing");
      ESP_GOTO_ON_ERROR(pcap_cmd_print_summary(pcap_cmd_rt.pcap_handle, stdout),
                        err, TAG, "pcap print summary failed");
    } else {
      FILE* fp;
      fp = fopen(pcap_cmd_rt.filename, "rb");
      ESP_GOTO_ON_FALSE(fp, ESP_FAIL, err, TAG, "open file failed");
      pcap_config_t pcap_config = {
          .fp = fp,
          .major_version = PCAP_DEFAULT_VERSION_MAJOR,
          .minor_version = PCAP_DEFAULT_VERSION_MINOR,
          .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
      };
      ESP_GOTO_ON_ERROR(
          pcap_new_session(&pcap_config, &pcap_cmd_rt.pcap_handle), err, TAG,
          "pcap init failed");
      ESP_GOTO_ON_ERROR(pcap_cmd_print_summary(pcap_cmd_rt.pcap_handle, stdout),
                        err, TAG, "pcap print summary failed");
      ESP_GOTO_ON_ERROR(pcap_del_session(pcap_cmd_rt.pcap_handle), err, TAG,
                        "stop pcap session failed");
    }
  }
  #endif  // CONFIG_SNIFFER_PCAP_DESTINATION_SD

  #if CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY
  /* Check if needs to be parsed and shown: "--summary" option */
  if (pcap_args.summary->count) {
    ESP_LOGI(TAG, "Memory is to be parsed");
    ESP_GOTO_ON_ERROR(pcap_cmd_print_summary(pcap_cmd_rt.pcap_handle, stdout),
                      err, TAG, "pcap print summary failed");
  }
  #endif  // CONFIG_SNIFFER_PCAP_DESTINATION_MEMORY

  if (pcap_args.open->count) {
    pcap_open(&pcap_cmd_rt);
  }
err:
  return ret;
}
#endif  // CONFIG_SNIFFER_PCAP_DESTINATION_JTAG

void register_pcap_cmd(void) {
#if !CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
  pcap_args.summary = arg_lit0(
      NULL, "summary", "option to parse and show the summary of .pcap file");
  pcap_args.file =
      arg_str1("f", "file", "<file>",
               "name of the file storing the packets in pcap format");
  pcap_args.close = arg_lit0(NULL, "close", "close .pcap file");
  pcap_args.open = arg_lit0(NULL, "open", "open .pcap file");
  pcap_args.end = arg_end(1);
#endif  // #if CONFIG_SNIFFER_PCAP_DESTINATION_JTAG
}
