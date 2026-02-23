# NeverMind Architecture

## M1 范围

M1 覆盖引导与早期初始化：BIOS + UEFI（经 GRUB multiboot2）、32-bit 入口切换到 x86_64 long mode、基础分页、early console、GDT/IDT/TSS 初始化与可观测 boot log。

## M2 范围

M2 引入内存管理基础能力：

1. PMM：基于 frame bitmap 的页分配器，支持从 multiboot2 memory map 初始化。
2. VMM：基于四级页表的最小映射接口，支持 4KB 页与 2MB 大页映射。
3. KHeap：`kmalloc/kfree` 初版，面向内核早期对象分配。

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

## 内存子系统（M2）

### 物理内存管理（PMM）

- 数据结构：固定上限位图（当前 128 GiB 物理内存覆盖）
- 算法：首次适配扫描 + frame bit 原子语义（M2 为单核早期，未加锁）
- 初始化：解析 multiboot2 `MB2_TAG_MMAP`，将 `type=1` 标记为可用页
- 保护区：默认保留低 1 MiB，避免 BIOS/实模式遗留区域被分配

### 虚拟内存管理（VMM）

- 页表级别：PML4 -> PDPT -> PD -> PT
- 功能接口：`vmm_map_page`、`vmm_map_2m`、`vmm_unmap_page`
- 策略：按需分配中间页表页（来自 PMM）

### 内核堆（KHeap）

- 结构：header + free list（早期版本）
- 行为：优先复用 free list，不足时向 PMM 申请页并按需切分
- 目标：满足 M2 之前子系统的动态分配需求

## 并发与锁（M1）

M1 尚未启用中断与多核并发路径，锁策略在 M3/M5 引入。当前所有初始化流程串行执行。

## 测试策略（M1/M2）

- 构建验证：`make all`
- 单元测试：`make test`（`pmm`/`kmalloc` 用户态模拟）
- 启动验证：`tests/smoke_m1.sh`
- 验证条件：QEMU 串口日志包含 `NeverMind: M2 mm boot ok`
- CI 失败策略：任一步骤失败即失败；失败时上传 QEMU 日志作为排障依据。
