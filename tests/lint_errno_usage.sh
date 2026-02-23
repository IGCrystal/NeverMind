#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

ERRNO_HEADER="include/nm/errno.h"

if [[ ! -f "$ERRNO_HEADER" ]]; then
  echo "errno usage lint failed: missing $ERRNO_HEADER"
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

def_file="$tmp_dir/defined.txt"
use_file="$tmp_dir/used.txt"
undef_file="$tmp_dir/undefined_used.txt"
unused_file="$tmp_dir/unused_defined.txt"

grep -E '^#define[[:space:]]+NM_E[A-Z0-9_]+' "$ERRNO_HEADER" | awk '{print $2}' | sort -u > "$def_file"

if [[ ! -s "$def_file" ]]; then
  echo "errno usage lint failed: no NM_E* definitions found in $ERRNO_HEADER"
  exit 1
fi

grep -Rho --include='*.c' 'NM_ERR[[:space:]]*(\s*NM_E[A-Z0-9_]\+\s*)' kernel userspace tests \
  | sed -E 's/.*NM_ERR[[:space:]]*\([[:space:]]*(NM_E[A-Z0-9_]+)[[:space:]]*\).*/\1/' \
  | sort -u > "$use_file"

comm -13 "$def_file" "$use_file" > "$undef_file"
comm -23 "$def_file" "$use_file" > "$unused_file"

if [[ -s "$undef_file" ]]; then
  echo "errno usage lint failed: found NM_E* usages without definitions in $ERRNO_HEADER"
  cat "$undef_file"
  exit 1
fi

if [[ -s "$unused_file" ]]; then
  echo "errno usage lint: warning: defined but currently unused errno symbols"
  cat "$unused_file"
fi

echo "errno usage lint: PASS"
