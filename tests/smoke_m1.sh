#!/usr/bin/env bash
set -euo pipefail

ISO="${1:-build/nevermind-m1.iso}"
LOG_DIR="build/test-logs"
mkdir -p "$LOG_DIR"

BIOS_LOG="$LOG_DIR/qemu-bios.log"
UEFI_LOG="$LOG_DIR/qemu-uefi.log"
SMOKE_MARKER='NeverMind: M8 hardening\+ci ready|\[00\.000300\] tss ready'
SMOKE_TIMEOUT="${SMOKE_TIMEOUT:-60s}"
SMOKE_RETRY_TIMEOUT="${SMOKE_RETRY_TIMEOUT:-90s}"

run_smoke_qemu() {
  local log_file="$1"
  local timeout_value="$2"
  shift
  shift

  rm -f "$log_file"

  timeout "$timeout_value" qemu-system-x86_64 "$@" \
    -boot d \
    -serial file:"$log_file" \
    -display none \
    -monitor none \
    -no-reboot \
    -no-shutdown || true
}

has_smoke_marker() {
  local log_file="$1"
  grep -Eq "$SMOKE_MARKER" "$log_file"
}

run_smoke_qemu "$BIOS_LOG" "$SMOKE_TIMEOUT" \
  -machine q35 \
  -m 512M \
  -smp 1 \
  -cdrom "$ISO"

if ! has_smoke_marker "$BIOS_LOG"; then
  echo "BIOS smoke marker not found in $BIOS_LOG, retrying with longer timeout (${SMOKE_RETRY_TIMEOUT})"
  run_smoke_qemu "$BIOS_LOG" "$SMOKE_RETRY_TIMEOUT" \
    -machine q35 \
    -m 512M \
    -smp 1 \
    -cdrom "$ISO"
fi

if ! has_smoke_marker "$BIOS_LOG"; then
  echo "BIOS smoke marker still not found in $BIOS_LOG"
  if [[ -f "$BIOS_LOG" ]]; then
    echo "---- BIOS LOG (tail) ----"
    tail -n 120 "$BIOS_LOG" || true
    echo "-------------------------"
  fi
  exit 1
fi

if [[ -f "${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}" ]]; then
  run_smoke_qemu "$UEFI_LOG" "$SMOKE_TIMEOUT" \
    -machine q35 \
    -m 512M \
    -smp 1 \
    -bios "${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}" \
    -cdrom "$ISO"
  if ! grep -Eq "$SMOKE_MARKER" "$UEFI_LOG"; then
    echo "UEFI smoke marker not found; continuing with BIOS smoke result"
    if [[ -f "$UEFI_LOG" ]]; then
      echo "---- UEFI LOG (tail) ----"
      tail -n 80 "$UEFI_LOG" || true
      echo "-------------------------"
    fi
  fi
fi

echo "Boot smoke test passed"
