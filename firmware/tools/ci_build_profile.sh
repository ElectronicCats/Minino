#!/usr/bin/env bash
# Build one firmware profile and enforce its flash budget. Used by CI and
# locally (`./tools/ci_build_profile.sh minino partitions.csv`).
set -euo pipefail

profile="${1:?profile (minino, sdr, bsides, ...)}"
partition="${2:?partition table CSV}"

if [ ! -f "$partition" ]; then
  echo "Partition table not found: $partition" >&2
  exit 1
fi

idf.py "@profiles/${profile}/profile" build
python3 tools/check_flash_fit.py \
  --partition-table "$partition" \
  --app-bin "build-${profile}/minino.bin" \
  --label "$profile"
