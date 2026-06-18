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

    Registers (0 cycles) → L1 Cache (1 cycle) → On-chip RAM (1-2 cycles) → SDRAM (20-50 cycles)

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
    make demo-06
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'layer4_software'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 06: Memory Hierarchy (On-chip RAM + SDRAM)")
print("=" * 60)

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: On-chip RAM vs SDRAM latency
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Latency Comparison ---")
print("  On-chip RAM (addr < 0x10000): 1 cycle")
print("  SDRAM (addr >= 0x10000): 5-25 cycles depending on row\n")

# Simple program: 4 loads from memory
prog = to_bytes([
    encode_i(0x100, ZERO, 0b100, T0, OP_LOAD),  # lbu from 0x100 (on-chip)
    encode_i(0x200, ZERO, 0b100, T1, OP_LOAD),  # lbu from 0x200 (on-chip)
    encode_i(0x300, ZERO, 0b100, T2, OP_LOAD),  # lbu from 0x300 (on-chip)
    encode_i(0x400, ZERO, 0b100, T3, OP_LOAD),  # lbu from 0x400 (on-chip)
    ECALL,
])

# Without memory hierarchy (flat, no latency)
cpu1 = hades.PipelinedCPU()
cpu1.load_program(list(prog))
cpu1.run()
print(f"  Flat memory (no hierarchy): {cpu1.get_cycles()} cycles")

# With memory hierarchy enabled (on-chip = 1 cycle per access)
cpu2 = hades.PipelinedCPU()
cpu2.set_mem_hierarchy_enabled(True)
cpu2.load_program(list(prog))
cpu2.run()
print(f"  With hierarchy (on-chip):   {cpu2.get_cycles()} cycles")
print(f"  SDRAM row hits:  {cpu2.get_sdram_row_hits()}")
print(f"  SDRAM row misses: {cpu2.get_sdram_row_misses()}")
print(f"  (All accesses in on-chip range → no SDRAM activity)")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: Row Buffer Side-Channel
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: SDRAM Row Buffer Effect ---")
print("  Sequential access (same row) = fast")
print("  Random access (different rows) = slow\n")

# Sequential loads (all in same row → row hits after first)
prog_seq = to_bytes([
    encode_i(0x100, ZERO, 0b100, T0, OP_LOAD),  # addr 0x100
    encode_i(0x101, ZERO, 0b100, T1, OP_LOAD),  # addr 0x101 (same row)
    encode_i(0x102, ZERO, 0b100, T2, OP_LOAD),  # addr 0x102 (same row)
    encode_i(0x103, ZERO, 0b100, T3, OP_LOAD),  # addr 0x103 (same row)
    ECALL,
])

cpu5 = hades.PipelinedCPU()
cpu5.set_mem_hierarchy_enabled(True)
cpu5.load_program(list(prog_seq))
cpu5.run()
cycles_seq = cpu5.get_cycles()
print(f"  Sequential (same row): {cycles_seq} cycles")
print(f"    Row hits: {cpu5.get_sdram_row_hits()}, misses: {cpu5.get_sdram_row_misses()}")

# Spread loads (different rows → row misses)
# Addresses far apart: 0x100, 0x500, 0x900, 0xD00 (different rows if row_bits=10 → 1KB rows)
prog_spread = to_bytes([
    encode_i(0x100, ZERO, 0b100, T0, OP_LOAD),  # row 0
    encode_i(0x500, ZERO, 0b100, T1, OP_LOAD),  # row 1
    encode_i(0x900, ZERO, 0b100, T2, OP_LOAD),  # row 2
    encode_i(0xD00, ZERO, 0b100, T3, OP_LOAD),  # row 3
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
print(f"  This difference reveals the ACCESS PATTERN!")

# ═══════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n{'='*60}")
print(f" Demo 06 complete.")