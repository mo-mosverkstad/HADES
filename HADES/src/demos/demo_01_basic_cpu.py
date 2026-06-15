"""
Demo 01: Minimal RV32I CPU — run a program, observe registers and power trace.

=== Contents of the demo ===
1. How to create a simulated RISC-V CPU
2. How to write machine code (instructions) and load them into the CPU
3. How to run the program and inspect the results (registers)

=== How to run ===
    make demo-01
"""

# Import the HADES simulator (compiled C++ engine exposed to Python)
import hades

# Import helper functions that encode RISC-V instructions into 32-bit integers
from riscvtools.asmpack import *

print("=" * 60)
print("DEMO 01: Minimal RV32I CPU Execution")
print("=" * 60)

# ─── Step 1: Write a program ─────────────────────────────────────────────
#
# We write 3 instructions + a halt (ECALL):
#
#   addi t0, zero, 0xAB    → load the value 0xAB into register t0
#   addi t1, zero, 0x55    → load the value 0x55 into register t1
#   xor  t2, t0, t1        → compute t0 XOR t1, store result in t2
#   ecall                   → halt the CPU (stop execution)
#
# In RISC-V:
#   - "addi" = add immediate (a constant number) to a register
#   - "xor"  = bitwise exclusive-OR of two registers
#   - "zero" = register x0, always contains 0
#   - "t0", "t1", "t2" = temporary registers (x5, x6, x7)
#
# The encode_i() and encode_r() functions convert these into 32-bit binary
# machine code, exactly as a real RISC-V assembler would.

prog = to_bytes([
    encode_i(0xAB, ZERO, 0b000, T0, OP_IMM),  # addi t0, zero, 0xAB → t0 = 0xAB
    encode_i(0x55, ZERO, 0b000, T1, OP_IMM),  # addi t1, zero, 0x55 → t1 = 0x55
    encode_r(0, T1, T0, 0b100, T2, OP_REG),   # xor  t2, t0, t1     → t2 = 0xFE
    ECALL,                                      # halt the CPU
])

# ─── Step 2: Create CPU and configure ────────────────────────────────────
#
# hades.CPU() creates a fresh simulated processor with:
#   - 32 registers (all zeroed, x0 hardwired to 0)
#   - 64KB of memory (all zeroed)
#   - A power leakage recorder (empty trace)
#
# set_noise(0.0) means we get perfect measurements (no random noise).
# In a real attack, there would be noise from the measurement equipment.

cpu = hades.CPU()

# ─── Step 3: Load program into memory and run ────────────────────────────
#
# load_program() copies our machine code into the CPU's memory at address
# 0x1000 (the default program start address). The CPU's program counter (PC)
# is set to 0x1000, so execution begins there.
#
# run() starts the fetch-decode-execute loop:
#   1. Fetch instruction from memory[PC]
#   2. Decode it (extract opcode, registers, immediate)
#   3. Execute it (compute result)
#   4. Write result to destination register → LEAKAGE RECORDED HERE
#   5. Advance PC to next instruction
#   Repeat until ECALL is encountered.

cpu.load_program(list(prog))
cpu.run()

# ─── Step 4: Inspect results ─────────────────────────────────────────────
#
# After execution, we can read any register or memory location.
# Register numbers: t0=x5, t1=x6, t2=x7

print(f"\nRegisters after execution:")
print(f"  t0 (x5)  = {cpu.get_reg(5):#06x}  (expected 0xab)")
print(f"  t1 (x6)  = {cpu.get_reg(6):#06x}  (expected 0x55)")
print(f"  t2 (x7)  = {cpu.get_reg(7):#06x}  (expected 0xfe = 0xAB ^ 0x55)")