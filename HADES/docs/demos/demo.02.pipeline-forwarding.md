# Demo 02 — 3-Stage Pipeline with Forwarding

This demo shows the HADES CPU running in pipelined mode, demonstrating data forwarding, load-use stalls, and branch penalties.

---

## 1. Build

```bash
source .venv/bin/activate
cd <project-path>/HADES
make engine
```

---

## 2. Run All Tests (Phase 1–4)

```bash
make test
```

Expected output includes both Phase 1 tests (single-cycle) and Phase 4 pipeline tests:

```
==================================================
HADES Phase 1 (regressional tests) - RV32I CPU Regression Tests
==================================================
  PASS: test_addi
  PASS: test_add_sub
  PASS: test_logic
  PASS: test_shift
  PASS: test_load_store
  PASS: test_branch
  PASS: test_jal
  PASS: test_lui
  PASS: test_loop_sum
  PASS: test_x0_hardwired
==================================================
Results: 10 passed, 0 failed, 10 total
All tests passed!

==================================================
HADES Phase 4 - Pipeline Tests
==================================================
  PASS: test_pipeline_basic
  PASS: test_pipeline_forwarding
  PASS: test_pipeline_load_use_stall
  PASS: test_pipeline_branch_penalty
  PASS: test_pipeline_cycles_gt_instret
  PASS: test_pipeline_loop
  PASS: test_perf_counters
  PASS: test_backward_compat_single_cycle
==================================================
Pipeline Results: 8 passed, 0 failed
```

---

## 3. Observe Pipeline Behavior Interactively

```bash
PYTHONPATH=build python3
```

```python
import hades

cpu = hades.PipelinedCPU()

# Program with data dependency (forwarding)
import struct
# addi t0, zero, 10
# addi t1, t0, 5      ← needs t0 (forwarded from EX, no stall)
# add  t2, t0, t1     ← needs both (forwarded)
# ecall
prog = [0x00a00293, 0x00528313, 0x006283b3, 0x00000073]
binary = b''.join(struct.pack('<I', x) for x in prog)
cpu.load_program(list(binary))
cpu.run()

perf = cpu.get_perf_counters()
print(f"Cycles:  {perf.mcycle}")
print(f"Instret: {perf.minstret}")
print(f"IPC:     {perf.minstret / perf.mcycle:.2f}")
print(f"Data stalls:   {perf.stalls_data}")
print(f"Branch stalls: {perf.stalls_branch}")
print(f"t0={cpu.get_reg(5)}, t1={cpu.get_reg(6)}, t2={cpu.get_reg(7)}")
```

Expected:
```
Cycles:  ~6
Instret: 3
IPC:     0.50
Data stalls:   0
Branch stalls: 0
t0=10, t1=15, t2=25
```

---

## 4. Observe Load-Use Stall

```python
cpu = hades.PipelinedCPU()
cpu.set_pipeline_enabled(True)

# Store 7 at address 0x40, then load + use immediately
cpu.load_data([7, 0, 0, 0], 0x40)

# lw t0, 0x40(zero)
# addi t1, t0, 1    ← load-use hazard → 1 stall
# ecall
prog = [0x04002283, 0x00128313, 0x00000073]
binary = b''.join(struct.pack('<I', x) for x in prog)
cpu.load_program(list(binary))
cpu.run()

perf = cpu.get_perf_counters()
print(f"Data stalls: {perf.stalls_data}")  # Should be 1
print(f"t0={cpu.get_reg(5)}, t1={cpu.get_reg(6)}")  # 7, 8
```

---

## 5. Compare Pipeline vs Single-Cycle

```python
import struct

prog = [0x00a00293, 0x00528313, 0x006283b3, 0x00000073]
binary = b''.join(struct.pack('<I', x) for x in prog)

# Single-cycle mode
cpu1 = hades.CPU()
cpu1.load_program(list(binary))
cpu1.run()
print(f"Single-cycle: {cpu1.get_cycles()} cycles, {cpu1.get_instret()} instret")

# Pipeline mode
cpu2 = hades.PipelinedCPU()
cpu2.load_program(list(binary))
cpu2.run()
perf = cpu2.get_perf_counters()
print(f"Pipeline:     {perf.mcycle} cycles, {perf.minstret} instret")
print(f"Pipeline IPC: {perf.minstret / perf.mcycle:.2f}")
```

---

## 6. Key Concepts Demonstrated

| Concept | What happens | Observable |
|---------|-------------|-----------|
| Forwarding | EX result bypassed to next EX | stalls_data = 0 |
| Load-use stall | Load result not ready for next EX | stalls_data += 1 |
| Branch penalty | IF/ID flushed on taken branch | stalls_branch += 1 |
| IPC < 1 | Pipeline startup + stalls | mcycle > minstret |

---

---

## What This Demo Proves

- 3-stage pipeline executes correctly (same results as single-cycle)
- Data forwarding eliminates stalls for ALU→ALU dependencies
- Load-use hazard detected and stalled exactly 1 cycle
- Branch penalty observable (1 cycle flush)
- Performance counters track cycles, instructions, and stalls
- Full backward compatibility with Phase 1-2


---

## Quick Run (One Command)

```bash
make demo-04
```

This runs `demos/demo_04_pipeline.py` which demonstrates forwarding, load-use stalls, branch penalties, and performance counters automatically.