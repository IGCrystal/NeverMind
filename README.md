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

## 环境准备（推荐）

- Ubuntu: `./scripts/setup_ubuntu.sh`
- Windows + WSL: `powershell -ExecutionPolicy Bypass -File .\scripts\setup_wsl.ps1`

也可使用容器复现：

```bash
docker build -t nevermind-dev .
docker run --rm -it -v "$PWD":/workspace nevermind-dev
```

若在 Windows 主机看到 `stddef.h/stdint.h` 缺失红线，请在 VS Code 选择 C/C++ 配置为 `NeverMind-WSL`，并在 WSL 内执行构建。

产物：

- `build/kernel.elf`
- `build/nevermind-m1.iso`

## 单元测试（M2）

```bash
make test
```

## 集成测试（M7）

```bash
make integration
```

## 用户态工具构建（M7）

```bash
make user-tools
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

## M6 网络工具源码

- `userspace/ping.c`
- `userspace/http_server.c`
- `userspace/http_client.c`

当前仓库提供源码与 socket API 对接示例；可使用 `make user-tools` 生成宿主侧演示二进制。

## 预期串口输出（片段）

```text
[00.000000] early console ready
[00.000100] gdt ready
[00.000200] idt ready
[00.000300] tss ready
[00.000400] mm ready: free_pages=... used_pages=...
[00.000500] proc+sched ready: policy=RR
[00.000600] syscall ready
[00.000700] fs ready: root=tmpfs
[00.000800] drivers ready: pit/kbd/pci/rtl8139
[00.000900] net ready: arp/ipv4/icmp/udp/tcp/socket
[00.001100] userspace shell ready
NeverMind kernel (M8)
arch: x86_64
boot: BIOS+UEFI via GRUB multiboot2
[00.001000] NeverMind: M8 hardening+ci ready
```

## 里程碑

- M1 (boot): 启动链路 + long mode + early init + CI smoke
- M2 (mm): 物理页分配（bitmap）+ 基础 VMM（4KB/2MB map）+ `kmalloc/kfree` + 单测
- M3 (proc): task_struct + kernel thread + RR/CFS(近似) + syscall 分发 + 调度单测
- M4 (fs): VFS + tmpfs + ext2 最小实现 + VFS 单测
- M5 (drivers): IRQ 框架 + PIT + keyboard + PCI 枚举 + RTL8139 骨架 + 单测
- M6 (network): ARP + IPv4 + ICMP + UDP + TCP + socket API + ping/http 工具源码
- M7 (userspace): shell(`ls/cat/echo`) + 管道/重定向 + 集成测试
- M8 (hardening+CI): klog+dmesg + release workflow + 安全/性能报告

## 最终声明模板（发布时更新）

本仓库于 YYYY-MM-DD 构建并在 QEMU vX.Y 上通过测试，工具链：clang vX.Y / gcc vX.Y，测试平台：Ubuntu XX，测试结果见 tests/results-YYYYMMDD/。

当前声明见 `FINAL_DECLARATION.md`。

## 贡献规范

详见 `CONTRIBUTING.md`（Conventional Commits、PR 流程、发布规范）。
