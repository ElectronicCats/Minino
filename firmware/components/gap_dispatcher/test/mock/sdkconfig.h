/* Host-test stub for the generated sdkconfig.h.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The dispatcher guards its Bluetooth half with CONFIG_BT_ENABLED and
 * includes the generated sdkconfig.h to read it. The host test has no
 * generated configuration, so this stands in for it.
 */
#pragma once

#define CONFIG_BT_ENABLED 1
