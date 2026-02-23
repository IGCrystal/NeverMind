#!/usr/bin/env bash
set -euo pipefail

ISO="${1:-build/nevermind-m1.iso}"

qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -smp 2 \
  -cdrom "$ISO" \
  -serial stdio \
  -display none \
  -monitor none \
  -no-reboot \
  -no-shutdown
