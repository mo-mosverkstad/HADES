"""
Demo 06: Multi-Core + Mutex - two CPUs sharing memory with synchronization.

=== What this demo shows ===
1. Two CPU cores running different programs simultaneously
2. Shared memory: both cores read/write the same RAM
3. Hardware mutex: atomic lock prevents race conditions
4. Contention timing: when one core holds the lock (executing protected code), the other spins (polling)
  Note: Spinning over interrupt because interrupt far more expensive (1000 cycles) vs 10-20 cycles

=== Background: Multi-Core ===
Modern CPUs have multiple cores sharing memory. This creates:
- Performance: parallel execution
- Problems: race conditions (two cores writing same address)
- Solution: hardware mutex (atomic test-and-set)

=== Background: Mutex Contention Side-Channel ===
When Core A holds the mutex and Core B tries to acquire it:
- Core B spins (loops checking the lock) → burns cycles
- The NUMBER of spin cycles reveals how long Core A held the lock
- If Core A is doing crypto, the lock hold time depends on the secret!

This is a CONTENTION-BASED side-channel attack.

=== HADES I/O Map for Multi-Core ===
  0xF060  Mutex register (shared between cores)
    Write: [31:1] OWNER, [0] VALUE (1=lock, 0=unlock)
    Read:  [31:1] current OWNER, [0] locked status

=== How to run ===
    make demo-06
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 06: Multi-Core + Mutex Contention")
print("=" * 60)

MUTEX_ADDR = 0xF060  # Mutex register address

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: Two cores running independently (no shared state)
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Independent Execution ---")
print("  Core 0: computes 1+2+...+5 = 15")
print("  Core 1: computes 10+20 = 30")
print("  Both run simultaneously, no interaction\n")

# Core 0 program: sum 1..5, store at 0x0080
prog0 = to_bytes([
    encode_i(0, ZERO, 0b000, T0, OP_IMM),     # sum = 0
    encode_i(1, ZERO, 0b000, T1, OP_IMM),     # i = 1
    encode_i(6, ZERO, 0b000, T2, OP_IMM),     # limit = 6
    encode_r(0, T1, T0, 0b000, T0, OP_REG),   # sum += i
    encode_i(1, T1, 0b000, T1, OP_IMM),       # i++
    encode_b(-8 & 0x1FFF, T2, T1, 0b100, OP_BRANCH),  # blt i, limit, -8
    # Store result at 0x0080
    encode_i(0x80, ZERO, 0b000, T3, OP_IMM),
    encode_s(0, T0, T3, 0b010, OP_STORE),     # sw sum, 0(t3)
    ECALL,
])

# Core 1 program: compute 10+20, store at 0x0084
prog1 = to_bytes([
    encode_i(10, ZERO, 0b000, T0, OP_IMM),
    encode_i(20, ZERO, 0b000, T1, OP_IMM),
    encode_r(0, T1, T0, 0b000, T2, OP_REG),   # t2 = 30
    encode_i(0x84, ZERO, 0b000, T3, OP_IMM),
    encode_s(0, T2, T3, 0b010, OP_STORE),     # sw t2, 0x84
    ECALL,
])

mc = hades.MultiCore()
mc.load_program(0, list(prog0), 0x1000)
mc.load_program(1, list(prog1), 0x2000)
mc.run(1000)

# Read results from shared memory
mem = mc.read_mem(0x0080, 8)
result0 = struct.unpack('<I', bytes(mem[0:4]))[0]
result1 = struct.unpack('<I', bytes(mem[4:8]))[0]

print(f"  Core 0 result (at 0x0080): {result0} (expected 15)")
print(f"  Core 1 result (at 0x0084): {result1} (expected 30)")
print(f"  Core 0 cycles: {mc.get_cycles(0)}, Core 1 cycles: {mc.get_cycles(1)}")
print(f"  Global cycles: {mc.get_global_cycles()}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Mutex — Core 0 locks, Core 1 tries to lock (contention)
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 2: Mutex Contention ---")
print("  Core 0: acquires mutex (owner=1), does work, releases")
print("  Core 1: tries to acquire mutex (owner=2), gets contention")
print(f"  Mutex at address 0x{MUTEX_ADDR:04X}\n")

# Core 0: lock mutex (owner=1, value=1 -> write 0x03), do work, unlock (write 0x02)
# Mutex write format: [31:1]=owner, [0]=value
# owner=1 → bits [31:1] = 1 → value to write = (1<<1)|1 = 3 (lock)
# unlock: (1<<1)|0 = 2
prog0_mutex = to_bytes([
    # Load mutex address
    encode_i(0x7FF, ZERO, 0b000, T4, OP_IMM),  # t4 = 0x7FF
    encode_i(0x7FF, T4, 0b000, T4, OP_IMM),    # t4 = 0x7FF + 0x7FF = 0xFFE (wrong)
])

prog0_mutex = to_bytes([
    # t4 = 0xF060 (mutex address) — need LUI + ADDI
    encode_u(0x0000F000, T4, OP_LUI),           # t4 = 0xF000
    encode_i(0x060, T4, 0b000, T4, OP_IMM),    # t4 = 0xF060
    # Lock: write (owner=1, value=1) = (1<<1)|1 = 3
    encode_i(3, ZERO, 0b000, T0, OP_IMM),       # t0 = 3
    encode_s(0, T0, T4, 0b010, OP_STORE),       # sw t0, 0(t4) -> lock
    # Do some work (5 NOPs)
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
    # Unlock: write (owner=1, value=0) = (1<<1)|0 = 2
    encode_i(2, ZERO, 0b000, T0, OP_IMM),       # t0 = 2
    encode_s(0, T0, T4, 0b010, OP_STORE),       # sw t0, 0(t4) -> unlock
    ECALL,
])

# Core 1: try to lock (owner=2, value=1) = (2<<1)|1 = 5
# Then read back to check if acquired
prog1_mutex = to_bytes([
    encode_u(0x0000F000, T4, OP_LUI),
    encode_i(0x060, T4, 0b000, T4, OP_IMM),     # t4 = 0xF060
    # Try lock: write (owner=2, value=1) = 5
    encode_i(5, ZERO, 0b000, T0, OP_IMM),       # t0 = 5
    encode_s(0, T0, T4, 0b010, OP_STORE),       # sw t0, 0(t4) → try lock
    # Read back mutex status
    encode_i(0, T4, 0b010, T1, OP_LOAD),        # lw t1, 0(t4) → read mutex
    # Try again (spin)
    encode_s(0, T0, T4, 0b010, OP_STORE),       # try lock again
    encode_i(0, T4, 0b010, T2, OP_LOAD),        # read again
    ECALL,
])

mc2 = hades.MultiCore()
mc2.load_program(0, list(prog0_mutex), 0x1000)
mc2.load_program(1, list(prog1_mutex), 0x2000)
mc2.run(1000)

print(f"  Core 0 cycles: {mc2.get_cycles(0)}")
print(f"  Core 1 cycles: {mc2.get_cycles(1)}")
print(f"  Mutex contentions: {mc2.get_mutex_contentions()}")
print(f"  Mutex final state: locked={mc2.get_mutex_locked()}, owner={mc2.get_mutex_owner()}")

# Core 1's t1 register shows what it read from mutex after first attempt
mutex_read1 = mc2.get_reg(1, 6)  # t1 = x6
mutex_read2 = mc2.get_reg(1, 7)  # t2 = x7
print(f"  Core 1 mutex read (1st attempt): 0x{mutex_read1:08X}")
print(f"  Core 1 mutex read (2nd attempt): 0x{mutex_read2:08X}")
print(f"    (bit 0 = locked, bits [31:1] = owner)")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: Timing Side-Channel from Contention
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: Contention Timing Side-Channel ---")
print("  If Core 0 holds mutex for N cycles (doing crypto),")
print("  Core 1's spin time reveals N → leaks execution time!\n")

# Core 0: lock, do variable work (10 vs 20 NOPs), unlock
for work_cycles in [5, 15]:
    nops = [encode_i(0, ZERO, 0b000, ZERO, OP_IMM)] * work_cycles
    prog0_var = to_bytes([
        encode_u(0x0000F000, T4, OP_LUI),
        encode_i(0x060, T4, 0b000, T4, OP_IMM),
        encode_i(3, ZERO, 0b000, T0, OP_IMM),
        encode_s(0, T0, T4, 0b010, OP_STORE),  # lock
    ] + nops + [
        encode_i(2, ZERO, 0b000, T0, OP_IMM),
        encode_s(0, T0, T4, 0b010, OP_STORE),  # unlock
        ECALL,
    ])

    # Core 1: just try to lock repeatedly
    prog1_spin = to_bytes([
        encode_u(0x0000F000, T4, OP_LUI),
        encode_i(0x060, T4, 0b000, T4, OP_IMM),
        encode_i(5, ZERO, 0b000, T0, OP_IMM),
        # Spin loop: try lock, check, repeat
        encode_s(0, T0, T4, 0b010, OP_STORE),  # try lock
        encode_i(0, T4, 0b010, T1, OP_LOAD),   # read status
        ECALL,
    ])

    mc3 = hades.MultiCore()
    mc3.load_program(0, list(prog0_var), 0x1000)
    mc3.load_program(1, list(prog1_spin), 0x2000)
    mc3.run(1000)

    print(f"  Core 0 work = {work_cycles} NOPs: "
          f"global_cycles={mc3.get_global_cycles()}, "
          f"contentions={mc3.get_mutex_contentions()}")

print(f"\n  -> More work under lock = more global cycles = observable!")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f" Demo 06 complete.")
print(f"\n Security insight: Mutex contention creates a TIMING side-channel.")
print(f"   If a crypto operation runs under a lock, another core can measure")
print(f"   how long the lock was held — revealing execution time of the secret")
print(f"   computation without any power measurement!")