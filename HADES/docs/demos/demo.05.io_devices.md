# Demo 05 — I/O Devices (Timer + UART + GPIO)

This demo shows memory-mapped I/O devices communicating between the CPU and Python (host).

---

## Prerequisites

- Phase 4 completed (memory hierarchy working)
- Engine built (`make engine`)

---

## Key Concepts

- **Memory-mapped I/O**: CPU accesses devices by reading/writing special addresses
- **UART**: Serial communication (Python ↔ CPU via FIFO buffers)
- **GPIO**: Parallel I/O pins (Python sets inputs, CPU sets outputs)
- **Timer**: Countdown counter with interrupt capability

---

## I/O Address Map

| Address | Device | Registers |
|---------|--------|-----------|
| 0xF000 | Timer | STATUS, CONTROL, PERIOD, SNAPSHOT |
| 0xF020 | UART | DATA (TX/RX), CONTROL |
| 0xF040 | GPIO | DATA, DIRECTION, INTERRUPTMASK, EDGECAPTURE |

---

## Quick Run (One Command)

```bash
make demo-05
```

This runs `src/demos/demo_05_io_devices.py` which demonstrates UART echo, GPIO XOR, and timer countdown.

---

## Expected Output

```
Part 1: UART sends 'Hi' → CPU echoes → Python receives ['H', 'i']
Part 2: GPIO input 0xAA → CPU XOR 0xFF → output 0x55
Part 3: Timer countdown with snapshot
```

---

## What This Demo Proves

- UART bidirectional communication works (Python ↔ CPU)
- GPIO input/output pins controllable from both sides
- Timer counts down and can be snapshot-read
- I/O addresses correctly routed (not confused with RAM)
- All previous phases still work (backward compatible)