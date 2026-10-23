#!/usr/bin/env bash
# Run every host test suite under firmware/. One source of truth for `make
# tests` and CI, so a new suite cannot be added to one and forgotten in the
# other.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"

export IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"

if [ ! -f "$IDF_PATH/components/unity/unity/src/unity.c" ]; then
  echo "Unity not found under IDF_PATH=$IDF_PATH" >&2
  exit 1
fi

mapfile -t SUITES < <(
  find . -type f -path '*/test/Makefile' \
    -not -path './components/openthread/*' \
    -not -path './managed_components/*' \
    -not -path './build*/*' \
    -not -path '*/build/*' \
    -not -path './.git/*' |
    sort
)

if [ "${#SUITES[@]}" -eq 0 ]; then
  echo "No host test suites found" >&2
  exit 1
fi

for makefile in "${SUITES[@]}"; do
  dir="$(dirname "$makefile")"
  echo "==> $dir"
  make -C "$dir" run-tests
done

echo "==> tools/test/test_flash_tools.py"
python3 tools/test/test_flash_tools.py

echo "All host test suites passed (${#SUITES[@]} C suites + flash tooling)"
