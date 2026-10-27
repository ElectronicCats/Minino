// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime signature overlay (additions/removals). Content: Task 8.
//
// The parser receives text already in memory, never a file path: reading the
// file from microSD (or receiving it over WiFi via the file browser) is the
// app's responsibility (Task 17). This keeps the parser 100% host-testable.
#pragma once
#include <stdint.h>

typedef struct {
  uint16_t added;
  uint16_t removed;
  uint16_t skipped;
} surv_overlay_stats_t;

// Discards the loaded overlay and restores the effective tables to match the
// compiled-in tables.
void surv_overlay_reset(void);

// Parses in-memory text line by line and rebuilds the effective OUI, keyword,
// UUID, and IE signature tables. A malformed line is counted as skipped and
// does not abort the rest of the text. text may be NULL or empty.
surv_overlay_stats_t surv_overlay_parse(const char* text);
