// SPDX-License-Identifier: MIT
//
// ALGORITMO DE REFERENCIA — NO SE COMPILA EN EL FIRMWARE.
//
// Extracto literal del constructor de firma de Information Elements de
// colonelpanichacks/flock-you (MIT), tomado de su main.cpp. Se vendoriza aqui
// con el unico proposito de correr un test diferencial: nuestro comparador
// binario (surv_ie.c) debe coincidir con este en TODA entrada.
//
// Por que existe este archivo: nuestra version reimplementa el algoritmo en
// tokens binarios para no formatear strings en el callback promiscuo. Esa
// reimplementacion se hizo leyendo el codigo de abajo, y una lectura
// equivocada produciria un detector que pasa todos sus tests y no reconoce una
// camara real jamas. La primera version, en efecto, omitia los tres parches de
// tramas malformadas y perdia 3020 de 25000 casos. Solo se detecto ejecutando
// los dos algoritmos en paralelo.
//
// No editar para "mejorarlo": su valor es ser el original.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IRAM_ATTR
#define nullptr NULL
// --- PACK method 2 PoC: Flock probe IE signature (primary allowlist only) ---

static const char FLOCK_PROBE_IE_SIG_PRIMARY[] =
    "2,12,127,221:506f9a16030103,45,191,221:0050f208000000";
static const char FLOCK_LITEON_IE_SIG_PREFIX[] = "221:506f9a16030103";

#define FY_IE_SSID          0
#define FY_IE_VENDOR        221
#define FY_PHANTOM_SKIP_CAP 16
#define FY_TLV_RESYNC_MAX   64

// Encode n raw bytes as lowercase hex pairs (no separator) for vendor IE
// tokens.
static void IRAM_ATTR fyHexNibbles(char* dst, const uint8_t* b, int n) {
  static const char hd[] = "0123456789abcdef";
  for (int i = 0; i < n; i++) {
    dst[i * 2] = hd[b[i] >> 4];
    dst[i * 2 + 1] = hd[b[i] & 0x0f];
  }
}
// True when ies[pos] starts vendor IE 221 with OUI 50:6f:9a (LiteON / Flock
// stack). Used to spot real IE boundaries inside corrupted/overflow TLV runs.
static bool IRAM_ATTR fyLiteonVendorAt(const uint8_t* ies, int len, int pos) {
  return pos + 9 <= len && ies[pos] == FY_IE_VENDOR && ies[pos + 1] == 7 &&
         ies[pos + 2] == 0x50 && ies[pos + 3] == 0x6f && ies[pos + 4] == 0x9a;
}
// Scan up to 32 bytes past a bogus TLV header for a real LiteON vendor IE —
// signals a phantom overflow (driver length/FCS skew) rather than end of frame.
static bool IRAM_ATTR fyPhantomLiteonAhead(const uint8_t* ies,
                                           int len,
                                           int pos) {
  int end = pos + 2 + 32;
  if (end > len - 1)
    end = len - 1;
  for (int j = pos + 2; j < end; j++) {
    if (fyLiteonVendorAt(ies, len, j))
      return true;
  }
  return false;
}
// True when declared IE length extends past the buffer but looks like a phantom
// tag-64/len-128 overflow with LiteON payload still present ahead in the
// buffer.
static bool IRAM_ATTR
fyIsPhantomOverflow(const uint8_t* ies, int len, uint8_t id, int elen, int i) {
  if (i + 2 + elen <= len)
    return false;
  if (elen > 200)
    return true;
  return id == 64 && elen == 128 && fyPhantomLiteonAhead(ies, len, i);
}
// After a TLV parse failure, slide forward up to FY_TLV_RESYNC_MAX bytes to
// find the next plausible IE header (id + len that fits in the buffer).
static int IRAM_ATTR fyTlvResync(const uint8_t* ies, int len, int start) {
  int end = start + FY_TLV_RESYNC_MAX;
  if (end > len - 1)
    end = len - 1;
  for (int j = start; j < end; j++) {
    int elen = (int) ies[j + 1];
    if (elen <= 200 && j + 2 + elen <= len)
      return j;
  }
  return -1;
}
// Append a comma-separated fragment to the growing IE signature string; fails
// if cap exceeded.
static bool IRAM_ATTR fySigAppend(char* out,
                                  size_t cap,
                                  size_t* pos,
                                  const char* part) {
  size_t plen = strlen(part);
  if (*pos != 0) {
    if (*pos + 1 >= cap)
      return false;
    out[(*pos)++] = ',';
  }
  if (*pos + plen >= cap)
    return false;
  memcpy(out + *pos, part, plen);
  *pos += plen;
  out[*pos] = '\0';
  return true;
}
// Append a non-vendor IE as its decimal tag id (e.g. "12", "127", "45").
static bool IRAM_ATTR fySigAppendTag(char* out,
                                     size_t cap,
                                     size_t* pos,
                                     uint8_t id) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", (unsigned) id);
  return fySigAppend(out, cap, pos, buf);
}
// Append vendor IE as "221:" + up to 8 payload bytes hex (matches PACK sig
// format).
static bool IRAM_ATTR fySigAppendVendor(char* out,
                                        size_t cap,
                                        size_t* pos,
                                        const uint8_t* body,
                                        int elen) {
  char buf[24];
  int take = elen < 8 ? elen : 8;
  buf[0] = '2';
  buf[1] = '2';
  buf[2] = '1';
  buf[3] = ':';
  fyHexNibbles(buf + 4, body, take);
  buf[4 + take * 2] = '\0';
  return fySigAppend(out, cap, pos, buf);
}

// Walk 802.11 IE TLVs and build comma-separated fingerprint: skip SSID (tag 0),
// encode vendor 221 payloads, otherwise record tag numbers. Handles phantom
// overflows and resync. Sets *complete when every byte was consumed.
static bool IRAM_ATTR fyBuildFlockIeSigFromIes(const uint8_t* ies,
                                               int len,
                                               char* out,
                                               size_t cap,
                                               bool* complete) {
  if (!ies || len < 2 || !out || cap < 2)
    return false;
  size_t pos = 0;
  out[0] = '\0';
  int i = 0;
  uint8_t phantomSkips = 0;
  while (i + 2 <= len) {
    uint8_t id = ies[i];
    int elen = (int) ies[i + 1];
    if (i + 2 + elen > len) {
      if (phantomSkips < FY_PHANTOM_SKIP_CAP &&
          fyIsPhantomOverflow(ies, len, id, elen, i)) {
        phantomSkips++;
        i += 2;
        continue;
      }
      int j = fyTlvResync(ies, len, i);
      if (j > i) {
        i = j;
        continue;
      }
      return false;
    }
    i += 2;
    if (id == FY_IE_SSID) {
      if (elen == 0) {
        while (i + 2 <= len && ies[i] == 0 && ies[i + 1] == 0)
          i += 2;
      } else {
        i += elen;
      }
      continue;
    }
    if (id == FY_IE_VENDOR && elen >= 4) {
      if (!fySigAppendVendor(out, cap, &pos, ies + i, elen))
        return false;
    } else {
      if (!fySigAppendTag(out, cap, &pos, id))
        return false;
    }
    i += elen;
  }
  if (complete)
    *complete = (i == len);
  return pos > 0;
}
// Normalize signature to "2,12,127,<rest from LiteON anchor>" when the LiteON
// vendor prefix is present but leading tags were truncated by parse skew.
static void IRAM_ATTR fyCanonicalizeFlockIeSig(char* sig, size_t cap) {
  if (!sig || cap < 8)
    return;
  if (strncmp(sig, "2,12,127,", 9) == 0 &&
      strstr(sig, FLOCK_LITEON_IE_SIG_PREFIX) != nullptr) {
    return;
  }
  const char* anchor = strstr(sig, FLOCK_LITEON_IE_SIG_PREFIX);
  if (!anchor)
    return;
  char tmp[128];
  int n = snprintf(tmp, sizeof(tmp), "2,12,127,%s", anchor);
  if (n > 0 && (size_t) n < cap)
    memcpy(sig, tmp, (size_t) n + 1);
}
// Normalize signature to "2,12,127,<rest from LiteON anchor>" when the LiteON
// vendor prefix is present but leading tags were truncated by parse skew.
static bool IRAM_ATTR fyPickBetterSig(const char* a,
                                      bool aComplete,
                                      const char* b,
                                      bool bComplete,
                                      char* out,
                                      size_t cap) {
  if (!a[0] && !b[0])
    return false;
  if (a[0] && !b[0]) {
    strncpy(out, a, cap - 1);
    out[cap - 1] = '\0';
    return true;
  }
  if (!a[0] && b[0]) {
    strncpy(out, b, cap - 1);
    out[cap - 1] = '\0';
    return true;
  }
  const char* pick = a;
  if (aComplete && !bComplete)
    pick = a;
  else if (!aComplete && bComplete)
    pick = b;
  else if (strlen(b) > strlen(a))
    pick = b;
  strncpy(out, pick, cap - 1);
  out[cap - 1] = '\0';
  return true;
}
// Build fingerprint from full body and from body+2 (skip leading empty SSID IE
// pair); merge, canonicalize, write to out.
static bool IRAM_ATTR fyBuildFlockIeSigFromProbeBody(const uint8_t* body,
                                                     int bodyLen,
                                                     char* out,
                                                     size_t cap) {
  if (!body || bodyLen < 2 || !out || cap < 16)
    return false;
  char sigA[128] = {0};
  char sigB[128] = {0};
  bool completeA = false, completeB = false;
  bool okA =
      fyBuildFlockIeSigFromIes(body, bodyLen, sigA, sizeof(sigA), &completeA);
  bool okB = false;
  if (bodyLen >= 2 && body[0] == 0 && body[1] == 0) {
    okB = fyBuildFlockIeSigFromIes(body + 2, bodyLen - 2, sigB, sizeof(sigB),
                                   &completeB);
  }
  char merged[128] = {0};
  if (!fyPickBetterSig(okA ? sigA : "", completeA, okB ? sigB : "", completeB,
                       merged, sizeof(merged))) {
    return false;
  }
  fyCanonicalizeFlockIeSig(merged, sizeof(merged));
  strncpy(out, merged, cap - 1);
  out[cap - 1] = '\0';
  return out[0] != '\0';
}
// True when sig exactly matches FLOCK_PROBE_IE_SIG_PRIMARY (drive-tested
// allowlist entry).
static bool IRAM_ATTR fyFlockIeSigIsPrimary(const char* sig) {
  return sig && strcmp(sig, FLOCK_PROBE_IE_SIG_PRIMARY) == 0;
}

static bool IRAM_ATTR fyProbeBodyFlockIeSigPrimary(const uint8_t* body,
                                                   int bodyLen) {
  char ieSig[128];
  int len = bodyLen;
  if (fyBuildFlockIeSigFromProbeBody(body, len, ieSig, sizeof(ieSig)) &&
      fyFlockIeSigIsPrimary(ieSig)) {
    return true;
  }
  if (len > 4 &&
      fyBuildFlockIeSigFromProbeBody(body, len - 4, ieSig, sizeof(ieSig)) &&
      fyFlockIeSigIsPrimary(ieSig)) {
    return true;
  }
  return false;
}

// Envoltorio no-static para el test diferencial.
bool surv_ref_is_primary(const uint8_t* body, int bodyLen) {
  return fyProbeBodyFlockIeSigPrimary(body, bodyLen);
}
