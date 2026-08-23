// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fingerprint de Information Elements de probe request.
//
// Procedencia: la firma y el manejo de tramas corruptas vienen de
// colonelpanichacks/flock-you (MIT), a su vez basado en la investigacion de
// DeFlockJoplin y en Pintor & Atzori, "Analysis of Wi-Fi Probe Requests
// Towards Information Element Fingerprinting", GLOBECOM 2022.
//
// Diferencia con el original: alli la firma se construye como string con
// snprintf y se compara con strcmp DENTRO del callback promiscuo. Aqui se
// compara token a token en binario, sin formateo en el hot path.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SURV_IE_VENDOR_TAG 221
#define SURV_IE_MAX_TOKS   16
#define SURV_IE_MAX_SIGS   8

typedef struct {
  uint8_t tag;
  uint8_t vlen;       // 0 si no es vendor
  uint8_t vendor[7];  // primeros 7 bytes del payload vendor
} surv_ie_tok_t;

// 1 = SSID IE presente con longitud 0; 0 = presente con longitud; -1 = ausente.
int surv_ie_is_wildcard_probe(const uint8_t* ies, int len);

// true si los IE reproducen la firma primaria de Flock.
bool surv_ie_matches_flock(const uint8_t* ies, int len);

// Anade una firma alternativa. La usa el overlay (Task 8) para seguir a las
// camaras cuando Flock actualice su firmware por OTA: una flota puede correr
// versiones mezcladas, asi que se admiten hasta SURV_IE_MAX_SIGS a la vez.
// Devuelve false si la tabla esta llena.
bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count);

// Descarta las firmas del overlay y deja solo la compilada.
void surv_ie_reset_signatures(void);
