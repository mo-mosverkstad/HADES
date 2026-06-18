"""
Demo 04: Memory Hierarchy — On-chip RAM vs SDRAM latency model.

=== What this demo shows ===
1. On-chip RAM: fast access (1-2 cycles)
2. SDRAM: slow access with row buffer effects
   - Same row (row hit): 5 cycles
   - Different row (row miss): 25 cycles
   - Periodic refresh: 50 cycle stall every 10000 accesses
3. How cache + memory hierarchy combine to amplify timing differences
4. Security implication: memory access patterns leak through timing

=== Background: Memory Hierarchy ===
Real systems have multiple levels of memory:

    Registers (0 cycles) -> L1 Cache (1 cycle) -> On-chip RAM (1-2 cycles) -> SDRAM (20-50 cycles)

Each level is larger but slower. The CPU stalls when data isn't in the
fastest level. These stalls create TIMING DIFFERENCES that depend on
the data being accessed — a side-channel!

=== Background: SDRAM Row Buffer ===
SDRAM is organized into rows. Accessing data in the currently-open row
is fast (row hit = 5 cycles). Accessing a different row requires closing
the current row and opening a new one (row miss = 25 cycles).

This means: SEQUENTIAL memory access is fast, RANDOM access is slow.
An attacker can detect access patterns by measuring timing!

=== How to run ===
    make demo-04
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 04: Memory Hierarchy (On-chip RAM + SDRAM)")
print("=" * 60)

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: On-chip RAM vs SDRAM latency
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Latency Comparison ---")
print("  On-chip RAM (addr < 0x10000): 1 cycle extra per access")
print("  SDRAM (addr >= 0x10000): 5-25 cycles extra per access\n")

# Simple program: 4 loads from on-chip memory (addr < 0x10000)
prog_onchip = to_bytes([
    encode_i(0x100, ZERO, 0b100, T0, OP_LOAD),  # lbu from 0x100 (on-chip)
    encode_i(0x200, ZERO, 0b100, T1, OP_LOAD),  # lbu from 0x200 (on-chip)
    encode_i(0x300, ZERO, 0b100, T2, OP_LOAD),  # lbu from 0x300 (on-chip)
    encode_i(0x400, ZERO, 0b100, T3, OP_LOAD),  # lbu from 0x400 (on-chip)
    ECALL,
])

# Without memory hierarchy (flat, no latency)
cpu1 = hades.PipelinedCPU()
cpu1.load_program(list(prog_onchip))
cpu1.run()
print(f"  Flat memory (no hierarchy): {cpu1.get_cycles()} cycles")

# With memory hierarchy enabled (on-chip = 1 cycle per access)
cpu2 = hades.PipelinedCPU()
cpu2.set_mem_hierarchy_enabled(True)
cpu2.load_program(list(prog_onchip))
cpu2.run()
print(f"  With hierarchy (on-chip):   {cpu2.get_cycles()} cycles")
print(f"    Extra = instruction fetches * 1 + data loads * 1")
print(f"  SDRAM row hits:  {cpu2.get_sdram_row_hits()}")
print(f"  SDRAM row misses: {cpu2.get_sdram_row_misses()}")
print(f"  (All accesses in on-chip range -> no SDRAM activity)")

# Now load from SDRAM range: use LUI to set base register to 0x10000
prog_sdram = to_bytes([
    encode_u(0x10000, S0, OP_LUI),               # lui s0, 0x10 -> s0 = 0x10000
    encode_i(0x000, S0, 0b100, T0, OP_LOAD),     # lbu t0, 0(s0) -> addr 0x10000 (SDRAM)
    encode_i(0x001, S0, 0b100, T1, OP_LOAD),     # lbu t1, 1(s0) -> addr 0x10001 (SDRAM, same row)
    encode_i(0x002, S0, 0b100, T2, OP_LOAD),     # lbu t2, 2(s0) -> addr 0x10002 (SDRAM, same row)
    encode_i(0x003, S0, 0b100, T3, OP_LOAD),     # lbu t3, 3(s0) -> addr 0x10003 (SDRAM, same row)
    ECALL,
])

cpu3 = hades.PipelinedCPU()
cpu3.set_mem_hierarchy_enabled(True)
cpu3.load_program(list(prog_sdram))
cpu3.run()
print(f"\n  With hierarchy (SDRAM sequential): {cpu3.get_cycles()} cycles")
print(f"  SDRAM row hits:  {cpu3.get_sdram_row_hits()} (same row = fast)")
print(f"  SDRAM row misses: {cpu3.get_sdram_row_misses()} (first access opens row)")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Row Buffer Side-Channel
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 2: SDRAM Row Buffer Side-Channel ---")
print("  Row size = 1KB (row_bits=10). Same row = addr>>10 matches.")
print("  Sequential access (same row) = 5 cycles (row hit)")
print("  Random access (different rows) = 25 cycles (row miss)\n")

# Sequential loads in SDRAM: all in same row
# 0x10000-0x10003 -> row = 0x10000>>10 = 64 (all same)
prog_seq = to_bytes([
    encode_u(0x10000, S0, OP_LUI),               # lui s0, 0x10 -> s0 = 0x10000
    encode_i(0x000, S0, 0b100, T0, OP_LOAD),     # addr 0x10000 (row 64)
    encode_i(0x001, S0, 0b100, T1, OP_LOAD),     # addr 0x10001 (row 64, hit)
    encode_i(0x002, S0, 0b100, T2, OP_LOAD),     # addr 0x10002 (row 64, hit)
    encode_i(0x003, S0, 0b100, T3, OP_LOAD),     # addr 0x10003 (row 64, hit)
    ECALL,
])

cpu5 = hades.PipelinedCPU()
cpu5.set_mem_hierarchy_enabled(True)
cpu5.load_program(list(prog_seq))
cpu5.run()
cycles_seq = cpu5.get_cycles()
print(f"  Sequential (same row): {cycles_seq} cycles")
print(f"    Row hits: {cpu5.get_sdram_row_hits()}, misses: {cpu5.get_sdram_row_misses()}")

# Spread loads in SDRAM: different rows (each 1KB apart)
# 0x10000 (row 64), 0x10400 (row 65), 0x10800 (row 66), 0x10C00 (row 67)
# imm12 is signed, max +2047. 0x400=1024, 0x800=2048 is too large for single imm.
# Use two base registers to reach far-apart addresses.
prog_spread = to_bytes([
    encode_u(0x10000, S0, OP_LUI),               # s0 = 0x10000
    encode_u(0x11000, S1, OP_LUI),               # s1 = 0x11000
    encode_i(0x000, S0, 0b100, T0, OP_LOAD),     # addr 0x10000 (row 64)
    encode_i(0x400, S0, 0b100, T1, OP_LOAD),     # addr 0x10400 (row 65)
    encode_i(0x000, S1, 0b100, T2, OP_LOAD),     # addr 0x11000 (row 68)
    encode_i(0x400, S1, 0b100, T3, OP_LOAD),     # addr 0x11400 (row 69)
    ECALL,
])

cpu6 = hades.PipelinedCPU()
cpu6.set_mem_hierarchy_enabled(True)
cpu6.load_program(list(prog_spread))
cpu6.run()
cycles_spread = cpu6.get_cycles()
print(f"  Spread (different rows): {cycles_spread} cycles")
print(f"    Row hits: {cpu6.get_sdram_row_hits()}, misses: {cpu6.get_sdram_row_misses()}")

print(f"\n  Timing difference: {cycles_spread - cycles_seq} cycles")
print(f"  Row misses cost 25 cycles vs 5 for hits -> pattern is visible!")

# ═══════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n{'='*60}")
print(f" Demo 04 complete.")
