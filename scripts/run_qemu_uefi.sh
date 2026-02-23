#!/usr/bin/env bash
set -euo pipefail

ISO="${1:-build/nevermind-m1.iso}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"

if [[ ! -f "$OVMF_CODE" ]]; then
  echo "OVMF firmware not found at $OVMF_CODE" >&2
  exit 2
fi

qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -smp 2 \
  -bios "$OVMF_CODE" \
  -cdrom "$ISO" \
  -serial stdio \
  -display none \
  -monitor none \
  -no-reboot \
  -no-shutdown
