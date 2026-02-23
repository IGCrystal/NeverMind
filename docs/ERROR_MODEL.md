# NeverMind 错误模型（阶段 1）

本文档定义内核内接口与 syscall 边界的错误返回约定，目标是消除魔法数字与语义漂移。

## 1. 返回值约定

- 成功：返回 `NM_OK`（即 `0`）或非负业务值（如字节数、pid、fd）。
- 失败：返回负值，形式为 `NM_ERR(NM_EXXX)`。
- 头文件：`include/nm/errno.h`。

## 2. 当前迁移状态

- syscall 分发层已统一未实现 syscall 返回 `NM_ERR(NM_ENOSYS)`。
- 历史路径中仍存在 `-1` 的通用失败返回；该值在当前阶段等价于 `NM_ERR(NM_EFAIL)`。
- 为保持兼容，现有测试依赖的 `-1` 语义暂不改变。
- 阶段 2（进行中）：`kernel/fs/vfs.c` 与 `kernel/net/{socket,tcp,udp}.c` 的公共错误返回已迁移到 `NM_ERR(...)` 常量写法（数值行为保持兼容）。
- 阶段 2（进行中）：`kernel/proc/{fd,task,exec_registry}.c` 的导出接口错误返回已迁移到 `NM_ERR(...)` 常量写法。
- 阶段 2（进行中）：`kernel/net/net.c` 与 `kernel/fs/{tmpfs,ext2}.c` 的错误返回已迁移到 `NM_ERR(...)` 常量写法。
- 阶段 2（进行中）：`kernel/drivers/irq.c` 的错误返回已迁移到 `NM_ERR(...)` 常量写法。
- 审计记录：`kernel/net/{arp,ipv4}.c` 当前不存在裸 `return -1` 路径，本轮无需改动。
- 阶段 2（进行中）：`kernel/drivers/rtl8139.c` 的错误返回已迁移到 `NM_ERR(...)` 常量写法。
- 审计记录：截至当前批次，`kernel/**/*.c` 中裸 `return -1` 路径已清零。

## 3. 子系统约束

- 子系统内部接口（`fs/*`, `net/*`, `proc/*`）允许在阶段 1 保留 `-1`，但新代码应优先使用错误常量。
- syscall 出口应尽量返回有语义的负错误码；若暂无法区分失败原因，返回 `NM_ERR(NM_EFAIL)`。

## 4. 后续阶段（计划）

- 阶段 2：将 `fs/net/proc` 公共接口逐步从裸 `-1` 迁移到明确错误码。
- 阶段 3：在用户态 ABI 层引入 errno 映射与一致的错误文档。

## 5. CI 防回退

- 新增 `tests/lint_error_model.sh`：扫描 `kernel/**/*.c`，禁止新增裸 `return -1;` 与 `return -38;`。
- 新增 `tests/lint_errno_usage.sh`：检查 `NM_E*` 的“使用必须有定义”，并报告“已定义但暂未使用”符号。
- `make test` 已接入 `make lint-error` 前置门禁。

## 6. 语义细分进度

- 参数非法场景开始返回 `NM_EINVAL`（示例：`sys_write/sys_read` 空缓冲区、`sys_pipe` 空指针参数）。
- 查找失败场景开始返回 `NM_ENOENT`（示例：`nm_exec_resolve_entry` 未命中）。
- 分配失败场景开始返回 `NM_ENOMEM`（示例：`tmpfs_reserve` 扩容分配失败）。
- 资源繁忙场景开始返回 `NM_EBUSY`（示例：IRQ bottom-half 队列满）。
