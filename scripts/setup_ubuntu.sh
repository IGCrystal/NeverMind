#!/usr/bin/env bash
set -euo pipefail

echo "[NeverMind] Installing build/test dependencies on Ubuntu..."
sudo apt-get update
sudo apt-get install -y \
  build-essential gcc clang lld binutils make \
  grub-pc-bin grub-common xorriso \
  qemu-system-x86 ovmf \
  cppcheck clang-tools \
  git ca-certificates

echo "[NeverMind] Done. Suggested validation commands:"
echo "  make clean all"
echo "  make test"
echo "  make integration"
echo "  make user-tools"
echo "  make smoke"
