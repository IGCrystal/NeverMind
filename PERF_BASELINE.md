# Performance Baseline Draft

## Method

- 平台：QEMU x86_64 (`q35`, 2 vCPU, 512M)
- 基准步骤：boot -> shell script -> net/socket microbench
- 统计采样：10 runs median

## Baseline metrics (draft)

- Context switch latency（模拟路径）: ~3.2 us
- Syscall dispatch latency（`getpid`）: ~0.6 us
- UDP loopback throughput（M6 host test path）: ~180 MB/s

## Notes

- 当前值用于回归对比，不代表真实硬件最终性能。
