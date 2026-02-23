# NeverMind Locking Order

This document defines the global lock ordering used in kernel subsystems to avoid ABBA deadlocks.

## Global Order

Acquire locks only in this order (top to bottom):

1. `pmm_lock` (physical memory allocator)
2. `vmm_lock` (page table mutations)
3. `proc_lock` (task table / current task)
4. `fd_lock` (fd objects / pipe tables)
5. `irq_lock` (irq table / BH queue metadata)
6. `sock_lock` (socket descriptor table)
7. `tcp_lock` (TCP connection table)
8. `udp_lock` (UDP port queues)
9. `net_lock` (net config/stat counters)

## Rules

- Never call back into a subsystem that can take an *earlier* lock while holding a later lock.
- Keep critical sections small; perform external callbacks outside locks.
- Avoid holding one lock while invoking blocking or potentially long-running paths.
- If a function needs multiple locks, refactor into:
  - lock-protected metadata phase,
  - unlocked work phase,
  - lock-protected commit phase.

## Current Exceptions

- No intentional exceptions are allowed at this time.
- If an exception becomes necessary, document it here with rationale and proof of deadlock safety.
