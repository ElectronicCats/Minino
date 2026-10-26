/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Osmosdr serial protocol parser and command handler.
 */

#include "osmosdr_serial.h"
#include <errno.h>
#include <string.h>
#ifdef ESP_PLATFORM
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#endif

#define CLAMP(x, low, high) \
  (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))
#ifndef MIN
  #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
  #define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

static inline uint16_t cpu_to_le16(uint16_t v) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return __builtin_bswap16(v);
#else
  return v;
#endif
}

static inline uint32_t cpu_to_le32(uint32_t v) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return __builtin_bswap32(v);
#else
  return v;
#endif
}

static inline uint16_t le16_to_cpu(uint16_t v) {
  return cpu_to_le16(v);
}

static inline uint32_t le32_to_cpu(uint32_t v) {
  return cpu_to_le32(v);
}

static inline uint64_t le64_to_cpu(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return __builtin_bswap64(v);
#else
  return v;
#endif
}

static inline uint32_t sys_get_le32(const uint8_t* src) {
  uint32_t val;
  memcpy(&val, src, sizeof(val));
  return le32_to_cpu(val);
}

static inline uint64_t sys_get_le64(const uint8_t* src) {
  uint64_t val;
  memcpy(&val, src, sizeof(val));
  return le64_to_cpu(val);
}

static inline void sys_put_le16(uint16_t val, uint8_t* dst) {
  val = cpu_to_le16(val);
  memcpy(dst, &val, sizeof(val));
}

static inline void sys_put_le32(uint32_t val, uint8_t* dst) {
  val = cpu_to_le32(val);
  memcpy(dst, &val, sizeof(val));
}

static inline void sys_put_le64(uint64_t val, uint8_t* dst) {
  val = le64_to_cpu(val);
  memcpy(dst, &val, sizeof(val));
}

void osmosdr_serial_init(osmosdr_serial_ctx_t* ctx) {
  if (ctx == NULL) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->settings.mode = ESDR_MODE_OFF;
  ctx->settings.bits = 8;
  ctx->settings.rx_gain = ESDR_GAIN_AUTO;
  ctx->settings.tx_gain = 8;
  ctx->settings.dgain = ESDR_DGAIN_DEFAULT;
  ctx->settings.freq_hz = (uint64_t) 2400U * 1000000ULL;
  ctx->settings.rate_hz = ESDR_RATE_MAX_HZ;
  ctx->settings.bandwidth_hz = 0;
  ctx->settings.corr_ppb = 0;
  ctx->settings.lo_mhz = 2400;
  ctx->settings.lo_offset_khz = 0;
  ctx->settings.center_hz = ctx->settings.freq_hz;
  ctx->settings.radio_dirty = true;
}

void osmosdr_serial_set_escape_cb(osmosdr_serial_ctx_t* ctx,
                                  osmosdr_escape_cb_t cb,
                                  void* arg) {
  if (ctx != NULL) {
    ctx->escape_cb = cb;
    ctx->escape_cb_arg = arg;
  }
}

uint32_t osmosdr_rate_round(uint32_t hz) {
  for (uint32_t r = ESDR_RATE_MAX_HZ;
       r > (ESDR_RATE_MAX_HZ >> (ESDR_RATE_COUNT - 1U)); r /= 2U) {
    if (2U * (uint64_t) hz * hz >= (uint64_t) r * r) {
      return r;
    }
  }
  return ESDR_RATE_MAX_HZ >> (ESDR_RATE_COUNT - 1U);
}

int osmosdr_handle_out(osmosdr_serial_ctx_t* ctx,
                       uint8_t req,
                       uint16_t value,
                       const uint8_t* data,
                       size_t len) {
  if (ctx == NULL) {
    return -EINVAL;
  }

  switch (req) {
    case ESDR_REQ_SET_MODE:
      if (value != ESDR_MODE_OFF && value != ESDR_MODE_RX &&
          value != ESDR_MODE_TX && value != ESDR_MODE_TEST_IN &&
          value != ESDR_MODE_TEST_OUT) {
        return -ENOTSUP;
      }
      /* The C6 has no transmitter: the dump engine reads the RF front end
         and there is no DAC to write samples to. The host is told the mode
         is not supported instead of being answered with silence. */
      if (value == ESDR_MODE_TX) {
        return -ENOTSUP;
      }
      ctx->settings.mode = value;
      return 0;

    case ESDR_REQ_SET_FREQ: {
      if (data == NULL || len != 8U) {
        return -EINVAL;
      }
      uint64_t hz = sys_get_le64(data);
      if (hz < (uint64_t) ESP_SDR_FREQ_MIN_MHZ * 1000000ULL ||
          hz > (uint64_t) ESP_SDR_FREQ_MAX_MHZ * 1000000ULL) {
        return -EINVAL;
      }
      ctx->settings.freq_hz = hz;

      int64_t adj_hz =
          (int64_t) hz - (int64_t) hz * ctx->settings.corr_ppb / 1000000000LL;
      ctx->settings.lo_mhz =
          (uint32_t) CLAMP(adj_hz / 1000000LL, (int64_t) ESP_SDR_FREQ_MIN_MHZ,
                           (int64_t) ESP_SDR_FREQ_MAX_MHZ);
      ctx->settings.lo_offset_khz =
          (int32_t) ((adj_hz - (int64_t) ctx->settings.lo_mhz * 1000000LL +
                      500) /
                     1000);
      ctx->settings.center_hz =
          ((uint64_t) ctx->settings.lo_mhz * 1000000ULL +
           (int64_t) ctx->settings.lo_offset_khz * 1000LL) +
          (int64_t) hz * ctx->settings.corr_ppb / 1000000000LL;
      ctx->settings.radio_dirty = true;
      return 0;
    }

    case ESDR_REQ_SET_SAMPLE_RATE: {
      if (data == NULL || len != 4U) {
        return -EINVAL;
      }
      uint32_t rate = sys_get_le32(data);
      if (rate == 0U) {
        return -EINVAL;
      }
      ctx->settings.rate_hz = osmosdr_rate_round(rate);
      return 0;
    }

    case ESDR_REQ_SET_RX_GAIN:
      if (value != ESDR_GAIN_AUTO && value > ESP_SDR_RX_GAIN_MAX) {
        return -EINVAL;
      }
      ctx->settings.rx_gain = value;
      ctx->settings.radio_dirty = true;
      return 0;

    case ESDR_REQ_SET_TX_GAIN:
      if (value > ESP_SDR_TX_GAIN_MAX) {
        return -EINVAL;
      }
      ctx->settings.tx_gain = value;
      return 0;

    case ESDR_REQ_SET_BANDWIDTH: {
      if (data == NULL || len != 4U) {
        return -EINVAL;
      }
      ctx->settings.bandwidth_hz = sys_get_le32(data);
      ctx->settings.radio_dirty = true;
      return 0;
    }

    case ESDR_REQ_SET_FORMAT:
      if (value != 8U && value != 16U) {
        return -EINVAL;
      }
      ctx->settings.bits = value;
      return 0;

    case ESDR_REQ_SET_FREQ_CORR: {
      if (data == NULL || len != 4U) {
        return -EINVAL;
      }
      int32_t corr = (int32_t) sys_get_le32(data);
      if (corr < -1000000 || corr > 1000000) {
        return -EINVAL;
      }
      ctx->settings.corr_ppb = corr;
      ctx->settings.radio_dirty = true;
      return 0;
    }

    case ESDR_REQ_SET_OPTIONS:
      return 0;

    case ESDR_REQ_SET_DIGITAL_GAIN:
      if (value != ESDR_DGAIN_DEFAULT &&
          (value > ESDR_DGAIN_MAX_DB || value % 6U != 0U)) {
        return -EINVAL;
      }
      ctx->settings.dgain = value;
      return 0;

    case ESDR_REQ_MININO:
      ctx->minino_escape_triggered = true;
      if (ctx->escape_cb != NULL) {
        ctx->escape_cb(ctx->escape_cb_arg);
      }
      return 0;

    default:
      return -ENOTSUP;
  }
}

int osmosdr_handle_in(osmosdr_serial_ctx_t* ctx,
                      uint8_t req,
                      uint8_t* buf,
                      size_t max) {
  if (ctx == NULL || buf == NULL || max == 0U) {
    return -EINVAL;
  }

  switch (req) {
    case ESDR_REQ_GET_INFO: {
      struct esdr_info info;
      memset(&info, 0, sizeof(info));
      info.magic = cpu_to_le32(ESDR_INFO_MAGIC);
      info.proto_version = cpu_to_le16(ESDR_PROTO_VERSION);
      info.info_size = cpu_to_le16((uint16_t) sizeof(info));
      strncpy(info.firmware, "osmosdr_serial minino",
              sizeof(info.firmware) - 1);
      info.freq_min_hz = (uint64_t) ESP_SDR_FREQ_MIN_MHZ * 1000000ULL;
      info.freq_max_hz = (uint64_t) ESP_SDR_FREQ_MAX_MHZ * 1000000ULL;
      info.rx_rate_max_hz = cpu_to_le32(ESDR_RATE_MAX_HZ);
      info.rx_rate_count = cpu_to_le16(ESDR_RATE_COUNT);
      info.rx_gain_max = cpu_to_le16(ESP_SDR_RX_GAIN_MAX);
      info.tx_gain_max = cpu_to_le16(ESP_SDR_TX_GAIN_MAX);
      info.block_bytes = cpu_to_le16(ESDR_BLOCK_BYTES);
      info.full_scale16 = cpu_to_le16(32767);

      size_t copy_len = MIN(max, sizeof(info));
      memcpy(buf, &info, copy_len);
      return (int) copy_len;
    }

    case ESDR_REQ_GET_STATE: {
      struct esdr_state st;
      memset(&st, 0, sizeof(st));
      st.mode = cpu_to_le16(ctx->settings.mode);
      st.bits = cpu_to_le16(ctx->settings.bits);
      st.options = 0;
      st.rx_gain = cpu_to_le16(ctx->settings.rx_gain);
      st.tx_gain = cpu_to_le16(ctx->settings.tx_gain);
      st.digital_gain_db =
          cpu_to_le16(ctx->settings.dgain == ESDR_DGAIN_DEFAULT
                          ? (ctx->settings.bits == 8U ? 24U : 0U)
                          : ctx->settings.dgain);
      st.freq_hz = ctx->settings.freq_hz;
      st.center_hz = ctx->settings.center_hz;
      st.sample_rate_hz = cpu_to_le32(ctx->settings.rate_hz);
      st.bandwidth_hz = cpu_to_le32(ctx->settings.bandwidth_hz);
      st.freq_corr_ppb =
          (int32_t) cpu_to_le32((uint32_t) ctx->settings.corr_ppb);
      st.lo_mhz = cpu_to_le32(ctx->settings.lo_mhz);
      st.lo_offset_khz =
          (int32_t) cpu_to_le32((uint32_t) ctx->settings.lo_offset_khz);

      size_t copy_len = MIN(max, sizeof(st));
      memcpy(buf, &st, copy_len);
      return (int) copy_len;
    }

    case ESDR_REQ_GET_STATS: {
      size_t copy_len = MIN(max, sizeof(ctx->stats));
      memcpy(buf, &ctx->stats, copy_len);
      return (int) copy_len;
    }

    default:
      return -ENOTSUP;
  }
}

bool osmosdr_serial_feed_byte(osmosdr_serial_ctx_t* ctx, uint8_t b) {
  if (ctx == NULL) {
    return false;
  }

  /* Escape pattern tracking: "MININO" */
  static const char MININO_CMD[] = "MININO";
  if (b == (uint8_t) MININO_CMD[ctx->escape_match_idx]) {
    ctx->escape_match_idx++;
    if (ctx->escape_match_idx == 6) {
      ctx->minino_escape_triggered = true;
      if (ctx->escape_cb != NULL) {
        ctx->escape_cb(ctx->escape_cb_arg);
      }
      ctx->escape_match_idx = 0;
      return true;
    }
  } else {
    ctx->escape_match_idx = (b == (uint8_t) MININO_CMD[0]) ? 1 : 0;
  }

  /* Binary frame header and data parsing */
  if (ctx->got < sizeof(ctx->raw_hdr)) {
    ctx->raw_hdr[ctx->got++] = b;

    /* Resynchronize on ESDR_SER_MAGIC */
    uint32_t target_magic = cpu_to_le32(ESDR_SER_MAGIC);
    while (ctx->got > 0 && ctx->got <= 4 &&
           memcmp(ctx->raw_hdr, &target_magic, ctx->got) != 0) {
      memmove(ctx->raw_hdr, ctx->raw_hdr + 1, --ctx->got);
    }

    if (ctx->got == sizeof(ctx->raw_hdr)) {
      uint16_t len = le16_to_cpu(ctx->hdr.len);
      if (ctx->hdr.req == ESDR_SER_TX_DATA) {
        ctx->tx_left = len;
        ctx->got = 0;
      } else if ((ctx->hdr.flags & ESDR_SER_IN) != 0 || len == 0) {
        ctx->frame_ready = true;
        return true;
      } else if (len > ESDR_SER_MAX_DATA) {
        ctx->hdr.len = 0;
        ctx->frame_ready = true;
        return true;
      }
    }
    return false;
  }

  /* Ingest payload */
  size_t payload_idx = ctx->got - sizeof(ctx->raw_hdr);
  if (payload_idx < sizeof(ctx->data)) {
    ctx->data[payload_idx] = b;
  }
  ctx->got++;

  if (ctx->got - sizeof(ctx->raw_hdr) >= le16_to_cpu(ctx->hdr.len)) {
    ctx->frame_ready = true;
    return true;
  }

  return false;
}

size_t osmosdr_serial_feed_bytes(osmosdr_serial_ctx_t* ctx,
                                 const uint8_t* buf,
                                 size_t len) {
  if (ctx == NULL || buf == NULL) {
    return 0;
  }
  size_t count = 0;
  for (size_t i = 0; i < len; i++) {
    if (osmosdr_serial_feed_byte(ctx, buf[i])) {
      count++;
    }
  }
  return count;
}

int osmosdr_serial_dispatch(osmosdr_serial_ctx_t* ctx) {
  if (ctx == NULL || !ctx->frame_ready) {
    return -EINVAL;
  }

  struct esdr_ser_hdr rep;
  memset(&rep, 0, sizeof(rep));
  rep.magic = cpu_to_le32(ESDR_SER_MAGIC);
  rep.req = ctx->hdr.req;

  uint16_t len = le16_to_cpu(ctx->hdr.len);
  int ret;

  if ((ctx->hdr.flags & ESDR_SER_IN) != 0U) {
    uint8_t in_buf[ESDR_SER_MAX_DATA];
    ret =
        osmosdr_handle_in(ctx, ctx->hdr.req, in_buf, MIN(len, sizeof(in_buf)));
    if (ret > 0) {
      rep.len = cpu_to_le16((uint16_t) ret);
      memcpy(ctx->out_buf + sizeof(rep), in_buf, (size_t) ret);
      ctx->out_len = sizeof(rep) + (size_t) ret;
    } else {
      rep.status = cpu_to_le16((uint16_t) (int16_t) ret);
      ctx->out_len = sizeof(rep);
    }
  } else {
    ret = osmosdr_handle_out(ctx, ctx->hdr.req, le16_to_cpu(ctx->hdr.value),
                             ctx->data, len);
    rep.status = cpu_to_le16((uint16_t) (int16_t) ret);
    ctx->out_len = sizeof(rep);
  }

  memcpy(ctx->out_buf, &rep, sizeof(rep));
  ctx->frame_ready = false;
  ctx->got = 0;
  return ret;
}

bool osmosdr_serial_is_minino_escape(const osmosdr_serial_ctx_t* ctx) {
  return ctx != NULL && ctx->minino_escape_triggered;
}

void osmosdr_serial_clear_minino_escape(osmosdr_serial_ctx_t* ctx) {
  if (ctx != NULL) {
    ctx->minino_escape_triggered = false;
    ctx->escape_match_idx = 0;
  }
}

size_t osmosdr_build_request_frame(uint8_t req,
                                   uint8_t flags,
                                   uint16_t value,
                                   const uint8_t* data,
                                   uint16_t len,
                                   uint8_t* out_buf,
                                   size_t max_out) {
  size_t payload_len =
      (data == NULL || (flags & ESDR_SER_IN) != 0U) ? 0U : (size_t) len;
  if (out_buf == NULL || max_out < sizeof(struct esdr_ser_hdr) + payload_len) {
    return 0;
  }
  struct esdr_ser_hdr* hdr = (struct esdr_ser_hdr*) out_buf;
  hdr->magic = cpu_to_le32(ESDR_SER_MAGIC);
  hdr->req = req;
  hdr->flags = flags;
  hdr->value = cpu_to_le16(value);
  hdr->len = cpu_to_le16(len);
  hdr->status = 0;

  if (data != NULL && payload_len > 0) {
    memcpy(out_buf + sizeof(struct esdr_ser_hdr), data, payload_len);
  }
  return sizeof(struct esdr_ser_hdr) + payload_len;
}

/* The C6 has no transmitter: the dump engine reads the RF front end and
   there is no DAC to write samples to, so the TX mode is refused above. */
