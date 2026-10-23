/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minino RF Spectrum Web Server & Live Canvas Dashboard
 */

#pragma once

#include "esp_err.h"

/* Not 80: the captive portal (main/apps/wifi/captive) and the File Manager web
   browser (main/modules/web_file_browser) both bind the default HTTP port, and
   Wi-Fi is up whenever the spectrum app runs. Sharing 80 made httpd_start()
   fail and the dashboard silently unavailable. */
#define RF_SPECTRUM_WEB_PORT 8080
#define RF_SPECTRUM_WEB_CTRL_PORT 32769

/* Custom header the /api/spectrum/control endpoint requires. A request that
   carries it is a CORS "preflighted" request, which a browser only sends for
   same-origin calls from a page we served. Without this, any page on the
   network could tamper with the running RF Spectrum session. */
#define RF_SPECTRUM_ADMIN_HDR "X-Minino-Admin"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the RF Spectrum HTTP server dashboard
 * Accessible at http://<minino-ip>:8080/spectrum
 */
esp_err_t rf_spectrum_web_start(void);

/**
 * @brief Stop the RF Spectrum HTTP server
 */
void rf_spectrum_web_stop(void);

#ifdef __cplusplus
}
#endif
