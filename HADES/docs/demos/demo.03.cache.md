# Demo 03 — L1 Cache

This demo shows how the L1 data cache can be implemented for the processor

---

## Prerequisites

- Phase 2 completed (pipeline working)
- Engine built (`make engine`)

---

## Key Concepts

- **Direct-mapped cache**: each memory address maps to exactly one cache line
- **Hit**: data already in cache → 1 cycle
- **Miss**: data not in cache → 20+ cycles (fetch from memory)

---

## Quick Run (One Command)

```bash
make demo-05
```

This runs `demos/demo_05_cache_attack.py` which demonstrates cache hits/misses and a cache timing attack on AES.

---

## Expected Output

```
Part 1: 20 * 2 misses = 40 extra cycles
Part 2: AES with cache shows ~31 D-cache misses
Part 3: Cache timing attack narrows key space
```

---

## What This Demo Proves

- Cache correctly tracks hits and misses
- Miss penalty adds measurable cycles
- AES S-box accesses create data-dependent cache patterns
- Timing differences are observable from Python
- Cache attack concept demonstrated (timing correlates with key)