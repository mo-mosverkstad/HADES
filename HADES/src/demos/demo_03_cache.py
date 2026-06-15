"""
Demo 05: L1 Cache — observe hits/misses

=== What this demo shows ===
1. How a direct-mapped cache works (hit = fast, miss = slow)
2. How cache misses create observable timing differences
3. How an attacker exploits cache timing to recover AES key bytes

=== Background: Direct-Mapped Cache ===
The DTEK-V L1 data cache:
- 2KB total, 64 lines, 32 bytes per line
- Direct-mapped: each memory address maps to exactly ONE cache line
- Address breakdown: [tag | index(6 bits) | offset(5 bits)]
  - offset = which byte within the 32-byte block
  - index  = which of the 64 cache lines
  - tag    = identifies which block is stored in that line

When the CPU loads a byte (LBU):
  - Compute index and tag from address
  - If cache[index].tag == tag -> HIT (1 cycle)
  - If mismatch -> MISS (20+ cycles to fetch from memory)

=== Background: Cache Attack on AES ===
The S-box table (256 bytes) spans 8 cache lines (256/32 = 8).
The cache line accessed depends on: (plaintext XOR key) >> 5

=== How to run ===
    make demo-03
"""
import hades
from riscvtools.asmpack import *

print("=" * 60)
print("DEMO 03: L1 Cache Behavior")
print("=" * 60)

# ═══════════════════════════════════════════════════════════════════════════
# Basic Cache Behavior test
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Cache Hit/Miss Behavior test ---")
print("  Cache: 2KB, 64 lines, 32B blocks, direct-mapped")
print("  Miss penalty: 20 cycles\n")

# Program: load from same address twice (first = miss, second = hit)

prog = to_bytes([
    encode_i(0x40, ZERO, 0b100, T0, OP_LOAD),  # lbu t0, 0x40(zero) → MISS
    encode_i(0x41, ZERO, 0b100, T1, OP_LOAD),  # lbu t1, 0x41(zero) → HIT (same block)
    encode_i(0x80, ZERO, 0b100, T2, OP_LOAD),  # lbu t2, 0x80(zero) → MISS (different line)
    encode_i(0x40, ZERO, 0b100, T3, OP_LOAD),  # lbu t3, 0x40(zero) → HIT (still cached)
    ECALL,
])

# Without cache
cpu_nc = hades.PipelinedCPU()
cpu_nc.set_cache_enabled(False)
cpu_nc.load_program(list(prog))
cpu_nc.run()
cycles_no_cache = cpu_nc.get_cycles()

# With cache
cpu_c = hades.PipelinedCPU()
cpu_c.set_cache_enabled(True)
cpu_c.set_miss_penalty(20)
cpu_c.load_program(list(prog))
cpu_c.run()
cycles_cache = cpu_c.get_cycles()

print(f"  Without cache: {cycles_no_cache} cycles (all accesses same speed)")
print(f"  With cache:    {cycles_cache} cycles (misses add 20 cycles each)")
print(f"  D-cache misses: {cpu_c.get_dcache_misses()}")
print(f"  Difference: {cycles_cache - cycles_no_cache} extra cycles from misses")
print(f"  Expected (Data cache): 2 misses * 20 = 40 extra cycles, (Instruction cache): 1 miss * 20 = 20")