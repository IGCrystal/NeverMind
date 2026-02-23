# Security & Reliability Audit Draft

## Automated checks

- `cppcheck` warnings/style/perf gate in CI
- `scan-build` static analyzer gate in CI
- Unit + integration test gates (`make test`, `make integration`)

## Hardening status

- 用户/内核基础边界：接口参数检查（`fs`, `socket`, `syscall`）
- 内核日志审计能力：`klog` ring buffer + `dmesg` 导出
- 驱动中断模型：top-half/bottom-half 分离，降低中断处理路径阻塞

## Remaining work (post-0.1)

- 完整 KASAN/UBSAN 内核级集成
- ASLR 与 stack canary 的系统级强制策略
- 更严格的权限模型与 LSM 钩子
