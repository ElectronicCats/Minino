#!/usr/bin/env bash
# Build one firmware profile and enforce its flash budget. Used by CI and
# locally (`./tools/ci_build_profile.sh minino partitions.csv [min_free_percent]`).
set -euo pipefail

profile="${1:?profile (minino, sdr, bsides, ...)}"
partition="${2:?partition table CSV}"
# Minimum free flash the image must leave in its app partition. Defaults to
# 5%; the 4MB bsides board overrides it because the combined Zigbee/Thread
# image leaves a thinner (but still safe) margin on that smaller flash.
min_free_percent="${3:-5}"

if [ ! -f "$partition" ]; then
  echo "Partition table not found: $partition" >&2
  exit 1
fi

idf.py "@profiles/${profile}/profile" build
python3 tools/check_flash_fit.py \
  --partition-table "$partition" \
  --app-bin "build-${profile}/minino.bin" \
  --min-free-percent "$min_free_percent" \
  --label "$profile"
