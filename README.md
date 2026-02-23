# NeverMind

NeverMind 是一个工程化的类 Linux 64-bit 单体内核项目（首要架构 x86_64）。

## 许可证

GPLv2-compatible，见 `LICENSE`。

## 工具链（M1）

- GCC >= 12 / Clang >= 17（当前默认使用 `gcc` + `ld`）
- GNU binutils >= 2.40
- GRUB tools: `grub-mkrescue`
- QEMU >= 8.0

## 快速构建

```bash
./build.sh
```

产物：

- `build/kernel.elf`
- `build/nevermind-m1.iso`

## 单元测试（M2）

```bash
make test
```

## 运行（BIOS）

```bash
make run-bios
```

## 运行（UEFI / OVMF）

```bash
make run-uefi
```

可通过环境变量覆盖固件路径：

```bash
OVMF_CODE=/usr/share/OVMF/OVMF_CODE.fd make run-uefi
```

## M1 Smoke Test

```bash
make smoke
```

## 预期串口输出（片段）

```text
[00.000000] early console ready
[00.000100] gdt ready
[00.000200] idt ready
[00.000300] tss ready
[00.000400] mm ready: free_pages=... used_pages=...
NeverMind kernel (M1)
arch: x86_64
boot: BIOS+UEFI via GRUB multiboot2
[00.001000] NeverMind: M2 mm boot ok
```

## 里程碑

- M1 (boot): 启动链路 + long mode + early init + CI smoke
- M2 (mm): 物理页分配（bitmap）+ 基础 VMM（4KB/2MB map）+ `kmalloc/kfree` + 单测
- M3 (proc): 任务模型、上下文切换、调度与 syscall
- M4 (fs): VFS + tmpfs + ext2
- M5 (drivers): timer/keyboard/pci/网卡基础
- M6 (network): Ethernet/IPv4/UDP/TCP 最小栈
- M7 (userspace): shell 与工具集 + 集成测试
- M8 (hardening+CI): 安全加固、发布与基线报告

## 最终声明模板（发布时更新）

本仓库于 YYYY-MM-DD 构建并在 QEMU vX.Y 上通过测试，工具链：clang vX.Y / gcc vX.Y，测试平台：Ubuntu XX，测试结果见 tests/results-YYYYMMDD/。
