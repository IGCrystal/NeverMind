# NeverMind Architecture

## M1 范围

M1 覆盖引导与早期初始化：BIOS + UEFI（经 GRUB multiboot2）、32-bit 入口切换到 x86_64 long mode、基础分页、early console、GDT/IDT/TSS 初始化与可观测 boot log。

## M2 范围

M2 引入内存管理基础能力：

1. PMM：基于 frame bitmap 的页分配器，支持从 multiboot2 memory map 初始化。
2. VMM：基于四级页表的最小映射接口，支持 4KB 页与 2MB 大页映射。
3. KHeap：`kmalloc/kfree` 初版，面向内核早期对象分配。

## M3 范围

M3 引入进程线程与系统调用骨架：

1. `task_struct`（寄存器集、CR3、FD 表、信号掩码、调度参数）
2. 调度器：RR 与近似 CFS
3. 上下文切换入口：x86_64 汇编 `nm_context_switch`
4. syscall 分发层：`getpid` 与 `write` 示例

## M4 范围

M4 引入文件系统基础层：

1. VFS：`vnode` + `file_ops` 抽象与统一文件 API
2. tmpfs：内存文件系统（目录树、文件读写、stat）
3. ext2：最小 inode/dir/block 元数据骨架与读写路径

## M5 范围

M5 引入设备驱动框架与中断驱动模型：

1. IRQ 注册与分发（top-half/bottom-half）
2. PIT 定时器驱动
3. 键盘中断驱动
4. PCI 枚举与设备发现
5. RTL8139 网卡最小驱动骨架

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

## 任务与调度（M3）

### 任务模型

- 数据结构：`struct nm_task`
- 必要字段：`regs`、`cr3`、`fd_table`、`signal_mask`、`sched`
- 线程类型：当前实现 kernel thread（用户态线程在 M7 接入）

### 调度策略

- RR：固定时间片（默认 4 ticks）循环选择可运行任务
- CFS(近似)：选择最小 `vruntime` 任务运行
- `vruntime` 更新：$\Delta v = \frac{ticks \times 1024}{weight(priority)}$

### syscall 框架

- 接口：`syscall_register` / `syscall_dispatch`
- 错误码：未注册 syscall 返回 `-ENOSYS`（当前编码 `-38`）
- 示例 syscall：`getpid`, `write(fd=1)`

## 文件系统（M4）

### VFS 抽象

- 抽象对象：`nm_vnode`、`nm_file`、`nm_file_ops`
- 接口：`fs_open/read/write/lseek/close/stat`
- 路径解析：`fs_path_split` + 根文件系统 `lookup/create`

### tmpfs

- 结构：内存 vnode 树（parent/children/sibling）
- 文件数据：可增长缓冲区
- 操作：支持 `open/read/write/stat`

### ext2 最小实现

- 元数据：`inode_meta`（mode/uid/gid/links/blocks）
- 目录项：vnode children 链接
- 限制：当前为最小可测实现骨架，磁盘块组完整语义在后续版本补齐

## 驱动与中断（M5）

### IRQ 框架

- 接口：`irq_register/irq_handle/irq_run_bottom_halves`
- 模型：中断 top-half 快速处理，延后工作由 bottom-half 队列执行
- 统计：每个 IRQ 维护 `hit_count`

### 定时器与输入

- PIT：可配置频率，注册在 IRQ32
- Keyboard：注册 IRQ33，扫描码转 ASCII（基础映射）

### PCI 与网卡

- PCI：扫描 bus/slot，提取 vendor/device/class 与 BAR0
- RTL8139：通过 PCI ID `10EC:8139` 探测，提供发送/接收接口骨架

## 测试策略（M1-M5）

- 构建验证：`make all`
- 单元测试：`make test`（`pmm`/`kmalloc` + `scheduler` + `vfs` + `irq/pci` 用户态模拟）
- 启动验证：`tests/smoke_m1.sh`
- 验证条件：QEMU 串口日志包含 `NeverMind: M5 drivers boot ok`
- CI 失败策略：任一步骤失败即失败；失败时上传 QEMU 日志作为排障依据。
