# Changelog

## [0.1.0] - 2026-02-23

### Added

- M1: BIOS+UEFI via GRUB, long mode, early console, GDT/IDT/TSS
- M2: PMM/VMM/kmalloc and memory tests
- M3: task/scheduler/syscall framework
- M4: VFS + tmpfs + ext2 minimal implementation
- M5: IRQ/PIT/keyboard/PCI/RTL8139 skeleton
- M6: ARP/IPv4/ICMP/UDP/TCP + socket API + ping/http sources
- M7: shell (`ls/cat/echo`) + redirection/pipe + integration tests
- M8: kernel log ring buffer + dmesg tool + release/audit/perf docs

## SemVer policy

- Major: incompatible ABI/layout changes
- Minor: backward-compatible subsystem features
- Patch: bugfixes and tooling updates
