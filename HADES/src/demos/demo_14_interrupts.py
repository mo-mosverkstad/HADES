"""
Demo 14: Interrupt Support - Timer IRQ with vectored handler.

=== What this demo shows ===
1. Setting up a timer interrupt handler via mtvec CSR
2. Timer fires after countdown, CPU jumps to handler
3. Handler increments a counter in memory, clears IRQ, returns via MRET
4. Main program resumes execution after interrupt

=== Interrupt flow ===
    Timer countdown expires
        -> Timer sets TO flag, IRQ pending
        -> CPU saves PC to mepc, sets mcause, jumps to mtvec
        -> Handler runs: increments counter, clears TO, MRET
        -> CPU restores PC from mepc, re-enables interrupts
        -> Main program continues

=== How to run ===
    make demo-14
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 14: Interrupt Support (Timer IRQ)")
print("=" * 60)

TIMER_BASE = 0xF000

# ===================================================================
# Part 1: Timer interrupt triggers handler
# ===================================================================

print("\n--- Part 1: Timer IRQ -> Handler -> MRET ---")
print("  Timer period=20 cycles, handler increments RAM[0x100]")

# Memory layout:
#   0x0100: interrupt counter (starts at 0)
#   0x1000: main program
#   0x2000: interrupt handler

# Main program:
#   - Set mtvec = 0x2000 (handler address)
#   - Enable interrupts
#   - Configure timer: period=20, continuous, ITO, start
#   - Loop (NOP sled) until counter reaches 3, then halt
main_prog = [
    # Set mtvec CSR (0x305) = 0x2000
    encode_u(0x00002000, T0, OP_LUI),          # t0 = 0x2000
    encode_i(0x305, T0, 0b001, ZERO, OP_SYSTEM),  # csrrw x0, mtvec, t0

    # Timer base in t4
    encode_u(0x0000F000, T4, OP_LUI),          # t4 = 0xF000

    # Set timer period = 20
    encode_i(20, ZERO, 0b000, T1, OP_IMM),     # t1 = 20
    encode_s(0x08, T1, T4, 0b010, OP_STORE),   # sw t1, 0x08(t4) = PERIOD_LO

    # Set timer CONTROL = ITO | CONT | START = 0b0111 = 7
    encode_i(7, ZERO, 0b000, T1, OP_IMM),      # t1 = 7
    encode_s(0x04, T1, T4, 0b010, OP_STORE),   # sw t1, 0x04(t4) = CONTROL

    # NOP loop: wait until RAM[0x100] >= 3
    # t3 = address 0x100
    encode_i(0x100, ZERO, 0b000, T3, OP_IMM),  # t3 = 0x100
    encode_i(3, ZERO, 0b000, T2, OP_IMM),      # t2 = 3
    # loop:
    encode_i(0, T3, 0b010, T5, OP_LOAD),       # lw t5, 0(t3) = counter
    encode_b(-4, T2, T5, 0b100, OP_BRANCH),    # blt t5, t2, loop (-4 bytes)
    ECALL,                                     # halt when counter >= 3
]

# Interrupt handler at 0x2000:
#   - Load counter from RAM[0x100], increment, store back
#   - Clear timer TO flag (write 0 to STATUS)
#   - MRET
handler_prog = [
    # Load counter from 0x100
    encode_i(0x100, ZERO, 0b000, T0, OP_IMM),  # t0 = 0x100
    encode_i(0, T0, 0b010, T1, OP_LOAD),       # t1 = mem[0x100]
    encode_i(1, T1, 0b000, T1, OP_IMM),        # t1 += 1
    encode_s(0, T1, T0, 0b010, OP_STORE),      # mem[0x100] = t1

    # Clear timer TO: write 0 to TIMER_STATUS (0xF000)
    encode_u(0x0000F000, T0, OP_LUI),          # t0 = 0xF000
    encode_s(0x00, ZERO, T0, 0b010, OP_STORE), # sw x0, 0(t0) = clear TO

    # MRET (opcode=SYSTEM, funct3=0, imm=0x302)
    encode_i(0x302, ZERO, 0b000, ZERO, OP_SYSTEM),  # mret
]

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.set_interrupts_enabled(True)

# Initialize counter to 0
cpu.load_data(list((0).to_bytes(4, 'little')), 0x100)

# Load main at 0x1000, handler at 0x2000
cpu.load_program(list(to_bytes(main_prog)), 0x1000)
cpu.load_data(list(to_bytes(handler_prog)), 0x2000)

cpu.run(100000)

counter = int.from_bytes(cpu.read_mem(0x100, 4), 'little')
print(f"\n  Interrupt counter: {counter}")
print(f"  Expected: >= 3")
print(f"  CPU halted: {cpu.is_halted()}")
print(f"  Cycles used: {cpu.get_cycles()}")
print(f"  Result: {'Correct' if counter >= 3 and cpu.is_halted() else 'FAILED'}")

# ===================================================================
# Part 2: Verify mepc saves/restores correctly
# ===================================================================

print("\n--- Part 2: Verify PC Save/Restore ---")
print("  Main program stores markers before/after interrupt window")

# Simpler test: main writes A=1, waits for interrupt, writes B=2
# If interrupt returns correctly, both A and B are set
prog2 = [
    # Set mtvec = 0x2000
    encode_u(0x00002000, T0, OP_LUI),
    encode_i(0x305, T0, 0b001, ZERO, OP_SYSTEM),  # csrrw x0, mtvec, t0

    # Write marker A = 1 at 0x200
    encode_i(1, ZERO, 0b000, T1, OP_IMM),
    encode_i(0x200, ZERO, 0b000, T2, OP_IMM),
    encode_s(0x00, T1, T2, 0b010, OP_STORE),   # mem[0x200] = 1

    # Timer: period=5, one-shot + ITO + START = 0b0101 = 5
    encode_u(0x0000F000, T4, OP_LUI),
    encode_i(5, ZERO, 0b000, T1, OP_IMM),
    encode_s(0x08, T1, T4, 0b010, OP_STORE),   # PERIOD = 5
    encode_i(5, ZERO, 0b000, T1, OP_IMM),
    encode_s(0x04, T1, T4, 0b010, OP_STORE),   # CONTROL = ITO|START (one-shot)

    # NOP sled (wait for interrupt to fire and return)
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop
    encode_i(0, ZERO, 0b000, ZERO, OP_IMM),    # nop

    # Write marker B = 2 at 0x204 (only reached if MRET worked)
    encode_i(2, ZERO, 0b000, T1, OP_IMM),
    encode_i(0x200, ZERO, 0b000, T2, OP_IMM),
    encode_s(0x04, T1, T2, 0b010, OP_STORE),   # mem[0x204] = 2

    ECALL,
]

# Handler: just clear TO and MRET
handler2 = [
    encode_u(0x0000F000, T0, OP_LUI),
    encode_s(0x00, ZERO, T0, 0b010, OP_STORE), # clear TO
    encode_i(0x302, ZERO, 0b000, ZERO, OP_SYSTEM),  # mret
]

cpu2 = hades.PipelinedCPU()
cpu2.set_io_enabled(True)
cpu2.set_interrupts_enabled(True)

cpu2.load_program(list(to_bytes(prog2)), 0x1000)
cpu2.load_data(list(to_bytes(handler2)), 0x2000)

cpu2.run(10000)

marker_a = int.from_bytes(cpu2.read_mem(0x200, 4), 'little')
marker_b = int.from_bytes(cpu2.read_mem(0x204, 4), 'little')
print(f"\n  Marker A (before interrupt): {marker_a}")
print(f"  Marker B (after return):     {marker_b}")
print(f"  CPU halted: {cpu2.is_halted()}")
correct = marker_a == 1 and marker_b == 2 and cpu2.is_halted()
print(f"  Result: {'Correct' if correct else 'FAILED'}")

# ===================================================================
# Part 3: Interrupts disabled - no handler invoked
# ===================================================================

print("\n--- Part 3: Interrupts Disabled ---")
print("  Timer fires but interrupts disabled -> no handler call")

cpu3 = hades.PipelinedCPU()
cpu3.set_io_enabled(True)
cpu3.set_interrupts_enabled(False)  # disabled

cpu3.load_data(list((0).to_bytes(4, 'little')), 0x100)
cpu3.load_program(list(to_bytes(main_prog)), 0x1000)
cpu3.load_data(list(to_bytes(handler_prog)), 0x2000)

# Run briefly - should not halt (counter never reaches 3)
cpu3.run(500)

counter3 = int.from_bytes(cpu3.read_mem(0x100, 4), 'little')
print(f"\n  Interrupt counter: {counter3} (expected 0)")
print(f"  CPU halted: {cpu3.is_halted()}")
# It won't halt because counter stays 0, loop runs forever
print(f"  Result: {'Correct' if counter3 == 0 else 'FAILED'}")

# ===================================================================
print(f"\n{'='*60}")
print(f" Demo 14 complete.")
print(f"\n Interrupt support enables CERBERUS OS to implement:")
print(f"   - Preemptive multitasking (timer interrupt -> context switch)")
print(f"   - I/O completion notification (disk/UART done -> wake process)")
print(f"   - Real-time deadlines (timer watchdog)")
print(f"   - Nested exception handling (page fault inside syscall)")
