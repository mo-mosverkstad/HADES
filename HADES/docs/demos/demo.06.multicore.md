# Demo 06 - Multi-Core + Mutex Contention

This demo shows two CPU cores running simultaneously, sharing memory and a hardware mutex, with observable contention timing. The mutex relies entirely on software cooperation and does not prevent a core from directly reading or writing shared memory. Programs are expected to acquire the mutex before accessing shared data and release it afterward. If a program ignores the mutex protocol, the hardware will not stop it, and race conditions can still occur.

---

## Prerequisites

- Phase 5 completed (I/O devices working)
- Engine built (`make engine`)

---

## Key Concepts

- **Multi-core**: 2 CPUs execute in round-robin, sharing memory
- **Hardware mutex**: atomic test-and-set prevents race conditions
- **Contention**: when one core holds the lock, the other spins (wastes cycles)
- **Timing side-channel**: lock hold time reveals execution duration of protected code

---

## Quick Run (One Command)

```bash
make demo-06
```

This runs `demos/demo_06_multicore.py` which demonstrates independent execution, mutex contention, and the contention timing side-channel.

---

## Expected Output

```
Part 1: Core 0 sum=15, Core 1 sum=30 (independent, shared memory)
Part 2: 2 mutex contentions observed
Part 3: 5 NOPs = 12 cycles, 15 NOPs = 22 cycles (10 more NOPs = 10 more cycles)
```

---

## What This Demo Proves

- Two cores execute simultaneously with shared memory
- Hardware mutex provides atomic lock/unlock
- Contention is detected and counted
- Lock hold time is directly observable via global cycle count
- Contention timing creates a side-channel (no power measurement needed!)