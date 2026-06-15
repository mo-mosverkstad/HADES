"""
Demo 02: 3-Stage Pipeline — observe forwarding, stalls, and performance counters.

=== What this demo shows ===
1. How a pipelined CPU executes instructions in overlapping stages
2. Data forwarding: how the CPU avoids stalls when possible
3. Load-use hazard: when a stall is unavoidable (1 cycle penalty)
4. Branch penalty: flushing the pipeline on a taken branch
5. Performance counters: measuring IPC (instructions per cycle)

=== Background: What is a Pipeline? ===
Without a pipeline (single-cycle CPU):
    Each instruction takes 1 cycle, but the cycle is LONG
    (must do fetch + decode + execute + memory + writeback all at once)

With a 3-stage pipeline (like DTEK-V):
    Stage 1 (IF/ID):  Fetch instruction from memory + decode it
    Stage 2 (EX):     Execute (ALU computation, branch decision)
    Stage 3 (MEM/WB): Memory access + write result to register

    Multiple instructions overlap:
    Cycle 1: [Instr A: IF/ID] [         ] [         ]
    Cycle 2: [Instr B: IF/ID] [Instr A: EX] [         ]
    Cycle 3: [Instr C: IF/ID] [Instr B: EX] [Instr A: MEM/WB]
    Cycle 4: [Instr D: IF/ID] [Instr C: EX] [Instr B: MEM/WB]

    Ideal throughput: 1 instruction per cycle (IPC = 1.0)

=== Background: Hazards ===
Sometimes the pipeline can't maintain IPC = 1.0:

1. DATA HAZARD (solved by forwarding):
   "addi t0, zero, 10" produces t0 in EX stage
   "addi t1, t0, 5"    needs t0 in its EX stage
   → Solution: FORWARD the result directly from EX to EX (no stall!)

2. LOAD-USE HAZARD (requires 1 stall):
   "lw t0, 0(addr)"    produces t0 in MEM/WB stage (memory read)
   "addi t1, t0, 1"    needs t0 in EX stage
   → Problem: result isn't ready yet! Must STALL 1 cycle.

3. BRANCH HAZARD (1 cycle penalty):
   "beq t0, t1, target" decides in EX stage
   But IF/ID already fetched the NEXT instruction (which may be wrong)
   → Solution: FLUSH the wrongly-fetched instruction (1 cycle wasted)

=== How to run ===
    make demo-04
"""
import sys, os, struct

import hades
from riscvtools.asmpack import *

print("=" * 60)
print("DEMO 02: 3-Stage Pipeline Behavior")
print("=" * 60)

# ═══════════════════════════════════════════════════════════════════════════
# Test 1: Data Forwarding (ALU → ALU, no stalls)
# ═══════════════════════════════════════════════════════════════════════════
#
# Program:
#   addi t0, zero, 10    → t0 = 10 (produced in EX stage)
#   addi t1, t0, 5       → needs t0 (FORWARDED from previous EX → no stall)
#   add  t2, t0, t1      → needs both t0 and t1 (both forwarded)
#   ecall
#
# Without forwarding: 2 stalls (waiting for t0, then t1)
# With forwarding:    0 stalls (results bypassed directly)

print("\n--- Test 1: ALU→ALU Forwarding (no stalls expected) ---")
prog = to_bytes([
    encode_i(10, ZERO, 0b000, T0, OP_IMM),   # t0 = 10
    encode_i(5, T0, 0b000, T1, OP_IMM),      # t1 = t0 + 5 = 15
    encode_r(0, T1, T0, 0b000, T2, OP_REG),  # t2 = t0 + t1 = 25
    ECALL,
])
cpu = hades.PipelinedCPU()
cpu.load_program(list(prog))
cpu.run(100)
perf = cpu.get_perf_counters()
print(f"  Results: t0={cpu.get_reg(5)}, t1={cpu.get_reg(6)}, t2={cpu.get_reg(7)}")
print(f"  Cycles: {perf.mcycle}, Instructions retired: {perf.minstret}")
print(f"  Data stalls: {perf.stalls_data} <- should be 0 (forwarding works!)")
print(f"  IPC: {perf.minstret/perf.mcycle:.2f}")

# ═══════════════════════════════════════════════════════════════════════════
# Test 2: Load-Use Hazard (1 stall cycle)
# ═══════════════════════════════════════════════════════════════════════════
#
# Program:
#   lw   t0, 0x40(zero)  → loads value from memory (result available in MEM/WB)
#   addi t1, t0, 1       → needs t0 in EX, but it's still in MEM/WB!
#   ecall
#
# The CPU must STALL for 1 cycle to wait for the load to complete.
# This is called a "load-use hazard" — the most common pipeline stall.
#
# Timeline:
#   Cycle 1: [LW: IF/ID]    [        ]    [        ]
#   Cycle 2: [ADDI: IF/ID]  [LW: EX]     [        ]
#   Cycle 3: [ADDI: STALL]  [LW: MEM/WB] [        ]  ← STALL! t0 not ready
#   Cycle 4: [ADDI: EX]     [        ]    [LW: done]  ← now t0 is available

print("\n--- Test 2: Load-Use Hazard (1 stall expected) ---")
prog = to_bytes([
    encode_i(0x40, ZERO, 0b010, T0, OP_LOAD),  # lw t0, 0x40(zero) → load from memory
    encode_i(1, T0, 0b000, T1, OP_IMM),         # addi t1, t0, 1 → uses t0 immediately!
    ECALL,
])
cpu = hades.PipelinedCPU()
cpu.load_data([42, 0, 0, 0], 0x40)  # Pre-store value 42 at address 0x40
cpu.load_program(list(prog))
cpu.run(100)
perf = cpu.get_perf_counters()
print(f"  Results: t0={cpu.get_reg(5)} (loaded 42), t1={cpu.get_reg(6)} (42+1=43)")
print(f"  Cycles: {perf.mcycle}, Instructions retired: {perf.minstret}")
print(f"  Data stalls: {perf.stalls_data} <- should be 1 (load-use hazard)")

# ═══════════════════════════════════════════════════════════════════════════
# Test 3: Branch Penalty (1 cycle flush)
# ═══════════════════════════════════════════════════════════════════════════
#
# Program:
#   addi t0, zero, 5     → t0 = 5
#   addi t1, zero, 5     → t1 = 5
#   beq  t0, t1, +8      → branch taken! (5 == 5) skip next instruction
#   addi t2, zero, 99    → THIS SHOULD BE SKIPPED
#   addi t3, zero, 1     → land here after branch
#   ecall
#
# When the branch is taken (decided in EX stage), the instruction that was
# already fetched into IF/ID (addi t2, zero, 99) must be FLUSHED.
# This wastes 1 cycle = "branch penalty".

print("\n--- Test 3: Branch Taken Penalty (1 flush expected) ---")
prog = to_bytes([
    encode_i(5, ZERO, 0b000, T0, OP_IMM),     # t0 = 5
    encode_i(5, ZERO, 0b000, T1, OP_IMM),     # t1 = 5
    encode_b(8, T1, T0, 0b000, OP_BRANCH),    # beq t0, t1, +8 -> TAKEN
    encode_i(99, ZERO, 0b000, T2, OP_IMM),    # SKIPPED (flushed)
    encode_i(1, ZERO, 0b000, T3, OP_IMM),     # t3 = 1 (executed)
    ECALL,
])
cpu = hades.PipelinedCPU()
cpu.load_program(list(prog))
cpu.run(100)
perf = cpu.get_perf_counters()
print(f"  Results: t2={cpu.get_reg(7)} (should be 0, was skipped)")
print(f"           t3={cpu.get_reg(28)} (should be 1, was executed)")
print(f"  Branch stalls: {perf.stalls_branch} <- should be >=1")

# ═══════════════════════════════════════════════════════════════════════════
# Test 4: Pipeline vs Single-Cycle Comparison
# ═══════════════════════════════════════════════════════════════════════════
#
# Same program, two modes:
# - Single-cycle: each instruction = 1 cycle (simple but slow clock)
# - Pipeline: overlapping execution (faster clock, more cycles but higher throughput)

print("\n--- Test 4: Pipeline vs Single-Cycle ---")
prog = to_bytes([
    encode_i(1, ZERO, 0b000, T0, OP_IMM),
    encode_i(2, ZERO, 0b000, T1, OP_IMM),
    encode_i(3, ZERO, 0b000, T2, OP_IMM),
    encode_i(4, ZERO, 0b000, T3, OP_IMM),
    ECALL,
])

# Single-cycle mode (Phase 1 behavior)
cpu_sc = hades.CPU()
cpu_sc.load_program(list(prog))
cpu_sc.run()
print(f"  Single-cycle: {cpu_sc.get_cycles()} cycles for 4 instructions + ecall")

# Pipeline mode
cpu_pl = hades.PipelinedCPU()
cpu_pl.load_program(list(prog))
cpu_pl.run(100)
perf = cpu_pl.get_perf_counters()
print(f"  Pipeline:     {perf.mcycle} cycles, {perf.minstret} instructions retired")
print(f"  Pipeline IPC: {perf.minstret/perf.mcycle:.2f}")

# ═══════════════════════════════════════════════════════════════════════════
# Test 5: Loop with Mixed Hazards
# ═══════════════════════════════════════════════════════════════════════════
#
# Compute sum = 1 + 2 + ... + 10 = 55
# This loop has:
# - ALU→ALU forwarding (sum += i, i++)
# - Taken branches (loop back) → branch penalties
# - No load-use hazards (no memory loads in loop body)

print("\n--- Test 5: Loop Sum 1..10 (forwarding + branch penalties) ---")
prog = to_bytes([
    encode_i(0, ZERO, 0b000, T0, OP_IMM),     # sum = 0
    encode_i(1, ZERO, 0b000, T1, OP_IMM),     # i = 1
    encode_i(11, ZERO, 0b000, T2, OP_IMM),    # limit = 11
    # Loop body (3 instructions, executed 10 times):
    encode_r(0, T1, T0, 0b000, T0, OP_REG),   # sum += i
    encode_i(1, T1, 0b000, T1, OP_IMM),       # i++
    encode_b(-8 & 0x1FFF, T2, T1, 0b100, OP_BRANCH),  # blt i, limit, loop
    ECALL,
])
cpu = hades.PipelinedCPU()
cpu.load_program(list(prog))
cpu.run(1000)
perf = cpu.get_perf_counters()
print(f"  Result: sum = {cpu.get_reg(5)} (expected 55)")
print(f"  Cycles: {perf.mcycle}, Instructions: {perf.minstret}")
print(f"  Data stalls: {perf.stalls_data} (expected 0, all forwarded)")
print(f"  Branch stalls: {perf.stalls_branch} (expected ~9, one per taken branch)")
print(f"  IPC: {perf.minstret/perf.mcycle:.2f}")

# ═══════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n{'='*60}")
print(f"Summary of Pipeline Effects:")
print(f"{'='*60}")
print(f"  - Forwarding eliminates stalls for ALU→ALU dependencies")
print(f"  - Load-use hazard: unavoidable 1-cycle stall (data not ready)")
print(f"  - Branch taken: 1-cycle penalty (wrong instruction fetched)")
print(f"  - IPC < 1.0 due to pipeline startup + hazards")
print(f" Demo 02 complete.")