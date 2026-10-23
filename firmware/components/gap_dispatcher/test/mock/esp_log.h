/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#define ESP_LOGE(tag, fmt, ...) ((void) (tag), (void) sizeof(fmt))
#define ESP_LOGW(tag, fmt, ...) ((void) (tag), (void) sizeof(fmt))
