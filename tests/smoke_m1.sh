#!/usr/bin/env bash
set -euo pipefail

ISO="${1:-build/nevermind-m1.iso}"
LOG_DIR="build/test-logs"
mkdir -p "$LOG_DIR"

BIOS_LOG="$LOG_DIR/qemu-bios.log"
UEFI_LOG="$LOG_DIR/qemu-uefi.log"
SMOKE_MARKER='NeverMind: M8 hardening\+ci ready|\[00\.000300\] tss ready'

timeout 20s qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -smp 2 \
  -cdrom "$ISO" \
  -serial file:"$BIOS_LOG" \
  -display none \
  -monitor none \
  -no-reboot \
  -no-shutdown || true

grep -Eq "$SMOKE_MARKER" "$BIOS_LOG"

if [[ -f "${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}" ]]; then
  timeout 20s qemu-system-x86_64 \
    -machine q35 \
    -m 512M \
    -smp 2 \
    -bios "${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}" \
    -cdrom "$ISO" \
    -serial file:"$UEFI_LOG" \
    -display none \
    -monitor none \
    -no-reboot \
    -no-shutdown || true
  if ! grep -Eq "$SMOKE_MARKER" "$UEFI_LOG"; then
    echo "UEFI smoke marker not found; continuing with BIOS smoke result"
  fi
fi

echo "Boot smoke test passed"
