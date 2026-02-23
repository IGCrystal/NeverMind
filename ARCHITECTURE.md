# NeverMind Architecture

## M1 范围

M1 覆盖引导与早期初始化：BIOS + UEFI（经 GRUB multiboot2）、32-bit 入口切换到 x86_64 long mode、基础分页、early console、GDT/IDT/TSS 初始化与可观测 boot log。

## 启动链路

1. BIOS 或 UEFI 固件进入 GRUB。
2. GRUB 依据 `grub/grub.cfg` 通过 multiboot2 加载 `kernel.elf`。
3. `boot/entry.S` 以 32-bit 模式启动：
   - 校验 multiboot2 magic
   - 构建 PML4/PDPT/PD（2MB 大页，1GiB 映射）
   - 同时映射 identity 与高半区 `0xFFFF800000000000`
   - 开启 PAE + LME + PG，远跳转进入 long mode
4. 进入 `kmain()`：初始化控制台与基础描述符表，输出 M1 成功标记。

## 地址空间布局（M1）

- 物理装载基址：`0x00100000`
- 虚拟高半区基址：`0xFFFF800000000000`
- `linker.ld` 将 `.boot` 保持低地址，内核主段链接到高地址并通过 `AT()` 对齐到低物理地址。

## 并发与锁（M1）

M1 尚未启用中断与多核并发路径，锁策略在 M3/M5 引入。当前所有初始化流程串行执行。

## 测试策略（M1）

- 构建验证：`make all`
- 启动验证：`tests/smoke_m1.sh`
- 验证条件：QEMU 串口日志包含 `NeverMind: M1 boot ok`
- CI 失败策略：任一步骤失败即失败；失败时上传 QEMU 日志作为排障依据。
