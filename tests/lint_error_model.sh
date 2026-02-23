#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

BAD1="$(grep -RIn --include='*.c' 'return -1;' kernel || true)"
BAD2="$(grep -RIn --include='*.c' 'return -38;' kernel || true)"

if [[ -n "$BAD1" || -n "$BAD2" ]]; then
  echo "error-model lint failed: found raw magic error returns in kernel sources"
  if [[ -n "$BAD1" ]]; then
    echo "-- raw 'return -1;' matches --"
    echo "$BAD1"
  fi
  if [[ -n "$BAD2" ]]; then
    echo "-- raw 'return -38;' matches --"
    echo "$BAD2"
  fi
  exit 1
fi

echo "error-model lint: PASS"
