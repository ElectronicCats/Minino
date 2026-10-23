/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Espectros calculados en el chip: el modo "On-Chip Spectrum" del visor de
 * ESPARGOS.
 *
 * En vez de mandar las muestras en crudo y que el navegador calcule la FFT,
 * la placa calcula el espectro y manda una lista de códigos de potencia. Es
 * lo que hace que el visor pueda refrescar a decenas de cuadros por segundo
 * en vez de a diez.
 *
 * Portado del firmware del ESPARGOS (GPL-3.0-or-later): main/common/
 * spectrum.c, spectrum_dc.h, spectrum_math.h y spectrum_fft.h, con el
 * formato descrito en su docs/spectrum.md.
 */

#ifndef ESDR_SPECTRUM_H_
#define ESDR_SPECTRUM_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* El mayor numero de bins que el firmware admite. */
#define ESDR_SPEC_MAX_FFT 2048u

/**
 * @brief Adquiere una rafaga y devuelve las palabras crudas.
 *
 * @param n       numero de muestras
 * @param rate    codigo de tasa (0 = 80 MS/s, 1 = 40 MS/s, 6 = 16 MS/s)
 * @param words   palabras crudas (I en los bits 9-0, Q en los 19-10)
 * @param elapsed_us micros que tardo la captura
 * @return true si la captura salio bien
 */
typedef bool (*esdr_spectrum_acquire_fn)(uint32_t n,
                                         uint32_t rate,
                                         const uint32_t** words,
                                         uint32_t* elapsed_us);

/**
 * @brief Atiende los comandos de espectro.
 *
 * @param line        la linea recibida
 * @param freq_mhz    la frecuencia que el cliente ha pedido
 * @param acquire     como se saca una rafaga
 * @return true si la linea era un comando de espectro (contestado o no)
 */
bool esdr_spectrum_command(const char* line,
                           uint32_t freq_mhz,
                           esdr_spectrum_acquire_fn acquire);

#ifdef __cplusplus
}
#endif

#endif /* ESDR_SPECTRUM_H_ */
