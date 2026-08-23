// SPDX-License-Identifier: GPL-3.0-or-later
// Overlay de firmas anadidas/quitadas en runtime. Contenido: Task 8.
//
// El parser recibe texto ya en memoria, nunca una ruta: leer el archivo de la
// microSD (o recibirlo por WiFi via el navegador de archivos) es
// responsabilidad de la app (Task 17). Asi el parser es 100% testeable en
// host.
#pragma once
#include <stdint.h>

typedef struct {
  uint16_t added;
  uint16_t removed;
  uint16_t skipped;
} surv_overlay_stats_t;

// Descarta el overlay cargado y vuelve a dejar las tablas efectivas iguales a
// las compiladas.
void surv_overlay_reset(void);

// Parsea texto en memoria linea a linea y reconstruye las tablas efectivas de
// OUIs, keywords, UUIDs y firmas IE. Una linea malformada se cuenta como
// saltada y no aborta el resto del texto. text puede ser NULL o vacio.
surv_overlay_stats_t surv_overlay_parse(const char* text);
