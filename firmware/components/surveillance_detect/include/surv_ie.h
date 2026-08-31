// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fingerprint de Information Elements de probe request.
//
// Procedencia: el algoritmo, la firma primaria y los parches de tramas
// malformadas vienen de colonelpanichacks/flock-you (MIT), a su vez basado en
// la investigacion de campo de DeFlockJoplin y en Pintor & Atzori, "Analysis of
// Wi-Fi Probe Requests Towards Information Element Fingerprinting", GLOBECOM
// 2022, doi:10.1109/GLOBECOM48099.2022.10001618.
//
// Diferencia con el original: alli la firma se construye como string con
// snprintf y se compara con strcmp DENTRO del callback promiscuo. Aqui se
// construye una lista de tokens y se compara en binario, sin formateo en el
// hot path. La equivalencia con el original se verifica con un test
// diferencial que ejecuta ambos algoritmos sobre las mismas entradas
// (test/test_surv_ie_diff.c).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SURV_IE_SSID_TAG   0
#define SURV_IE_VENDOR_TAG 221
// Maximo de tokens de UNA firma (compilada u overlay). Limita el almacen de
// s_extra y cuantos tokens admite una linea +iesig del overlay.
#define SURV_IE_MAX_TOKS   16
#define SURV_IE_MAX_SIGS   8
#define SURV_IE_VENDOR_MAX 8  // la referencia codifica hasta 8 bytes de vendor
// Limite de tokens al RECORRER una trama. No es un limite de firma: la
// referencia construye la firma en un buffer de 128 caracteres y falla cuando
// el token no cabe; 64 es el maximo de tokens que caben en esos 128 caracteres
// (el token minimo es 1 digito + coma), asi que este cap no se alcanza antes
// que el presupuesto de la referencia y el comparador la sigue al pie de la
// letra en tramas largas. Un cap mas pequeno diverge de ella en tramas con
// muchas IE pequenas por delante del ancla de LiteON.
#define SURV_IE_WALK_CAP 64

typedef struct {
  uint8_t tag;
  uint8_t vlen;  // bytes de vendor significativos; 0 si no lo es
  uint8_t vendor[SURV_IE_VENDOR_MAX];
} surv_ie_tok_t;

// 1 = SSID IE presente con longitud 0; 0 = presente con longitud; -1 = ausente.
int surv_ie_is_wildcard_probe(const uint8_t* ies, int len);

// true si los IE reproducen la firma de Flock (la compilada o alguna del
// overlay).
bool surv_ie_matches_flock(const uint8_t* ies, int len);

// Anade una firma alternativa; false si la tabla esta llena o los datos no
// valen.
bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count);

// Descarta las firmas del overlay y deja solo la compilada.
void surv_ie_reset_signatures(void);
