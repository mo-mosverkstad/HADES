# Demo 04 - Memory Hierarchy (On-chip RAM + SDRAM)

This demo shows how the memory hierarchy creates timing differences based on access patterns, exploitable as a side-channel.

---

## Prerequisites

- Phase 3 completed (cache working)
- Engine built (`make engine`)

---

## Key Concepts

- **On-chip RAM**: fast (1-2 cycles), small (64KB)
- **SDRAM**: slow (5-25 cycles), large, has row buffer
- **Row hit**: accessing same row as previous = 5 cycles
- **Row miss**: accessing different row = 25 cycles (must activate new row)
- **Refresh**: periodic 50-cycle stall every 10000 accesses
- **Side-channel**: sequential vs random access patterns have measurably different timing

---

## Quick Run (One Command)

```bash
make demo-06
```

This runs `demos/demo_06_memory_hierarchy.py` which demonstrates on-chip vs SDRAM latency and the row buffer side-channel.

---

## Expected Output

```
Part 1: On-chip accesses add 1 cycle each
Part 2: AES with cache + hierarchy shows memory latency effects
Part 3: Sequential vs spread access = ~48 cycle timing difference
```

---

## What This Demo Proves

- On-chip RAM has low, predictable latency
- SDRAM row buffer creates data-dependent timing
- Sequential access is fast (row hits), random is slow (row misses)