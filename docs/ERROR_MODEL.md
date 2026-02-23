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

## 3. 子系统约束

- 子系统内部接口（`fs/*`, `net/*`, `proc/*`）允许在阶段 1 保留 `-1`，但新代码应优先使用错误常量。
- syscall 出口应尽量返回有语义的负错误码；若暂无法区分失败原因，返回 `NM_ERR(NM_EFAIL)`。

## 4. 后续阶段（计划）

- 阶段 2：将 `fs/net/proc` 公共接口逐步从裸 `-1` 迁移到明确错误码。
- 阶段 3：在用户态 ABI 层引入 errno 映射与一致的错误文档。
