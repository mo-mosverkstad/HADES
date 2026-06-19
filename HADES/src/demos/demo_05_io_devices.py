"""
Demo 05: I/O Devices — Timer, UART, and GPIO.

=== What this demo shows ===
1. Timer: countdown, timeout detection, cycle measurement
2. UART: send data from Python → CPU, read CPU output back in Python
3. GPIO: set input pins from Python, read output pins set by CPU

=== Background: Memory-Mapped I/O ===
In real embedded systems, peripherals (timer, UART, GPIO) are controlled
by reading/writing specific memory addresses. The CPU doesn't know it's
talking to a device — it just does LW/SW to special addresses.

HADES I/O address map:
  0xF000-0xF01F  Timer (countdown, IRQ, snapshot)
  0xF020-0xF03F  UART  (FIFO TX/RX, status)
  0xF040-0xF05F  GPIO  (data, direction, edge capture)

=== How to run ===
    make demo-05
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 05: I/O Devices (Timer + UART + GPIO)")
print("=" * 60)

# I/O addresses (matching io_bus.h registration)
TIMER_BASE = 0xF000
UART_BASE  = 0xF020
GPIO_BASE  = 0xF040

# Helper: encode LUI + ADDI to load a 32-bit address into a register
def load_addr(rd, addr):
    """Generate instructions to load a 16-bit address into rd."""
    # For addresses < 0x800, a single ADDI from zero works
    # For addresses >= 0x800, need LUI + ADDI
    if addr < 0x800:
        return [encode_i(addr, ZERO, 0b000, rd, OP_IMM)]
    else:
        upper = (addr + 0x800) & 0xFFFFF000  # adjust for sign extension
        lower = addr - upper
        if lower < 0: lower += 0x1000
        instrs = [encode_u(upper, rd, OP_LUI)]
        if lower != 0:
            instrs.append(encode_i(lower & 0xFFF, rd, 0b000, rd, OP_IMM))
        return instrs

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: UART - Send data to CPU, CPU echoes it back
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: UART Communication ---")
print("  Python sends 'Hi' to CPU via UART RX FIFO")
print("  CPU reads bytes and writes them back to UART TX")
print(f"  UART DATA register at 0x{UART_BASE:04X}\n")

# Program: read 2 bytes from UART, write them back
# UART DATA at 0xF020: read pops RX, write pushes TX
uart_addr_instrs = load_addr(T3, UART_BASE)
prog = to_bytes(
    uart_addr_instrs + [
        # Read byte 1 from UART
        encode_i(0, T3, 0b010, T0, OP_LOAD),    # lw t0, 0(t3) -> read UART DATA
        # Write byte 1 back to UART
        encode_s(0, T0, T3, 0b010, OP_STORE),   # sw t0, 0(t3) -> write UART DATA
        # Read byte 2
        encode_i(0, T3, 0b010, T1, OP_LOAD),    # lw t1, 0(t3)
        # Write byte 2 back
        encode_s(0, T1, T3, 0b010, OP_STORE),   # sw t1, 0(t3)
        ECALL,
    ]
)

cpu = hades.PipelinedCPU()
cpu.uart_send([ord('H'), ord('i')])  # Python -> CPU RX FIFO
cpu.load_program(list(prog))
cpu.run()

# Read what CPU sent back
output = cpu.uart_recv()
print(f"  Sent to CPU:     {[chr(b) for b in [ord('H'), ord('i')]]}")
print(f"  Received from CPU: {[chr(b) for b in output[:2]] if len(output) >= 2 else output}")
print(f"  Raw bytes: {output}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: GPIO - Set input, read output
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 2: GPIO ---")
print("  Python sets GPIO input pins = 0xAA")
print("  CPU reads input, XORs with 0xFF, writes to output")
print(f"  GPIO DATA register at 0x{GPIO_BASE:04X}\n")

# Program: read GPIO input, XOR with 0xFF, write to GPIO output
gpio_addr_instrs = load_addr(T3, GPIO_BASE)
prog = to_bytes(
    gpio_addr_instrs + [
        # Read GPIO input
        encode_i(0, T3, 0b010, T0, OP_LOAD),    # lw t0, 0(t3) -> GPIO DATA (input)
        # XOR with 0xFF
        encode_i(0xFF, T0, 0b100, T1, OP_IMM),  # xori t1, t0, 0xFF
        # Write to GPIO output
        encode_s(0, T1, T3, 0b010, OP_STORE),   # sw t1, 0(t3) -> GPIO DATA (output)
        ECALL,
    ]
)

cpu = hades.PipelinedCPU()
cpu.gpio_set_input(0xAA)  # Python sets input pins
cpu.load_program(list(prog))
cpu.run()

gpio_out = cpu.gpio_get_output()
print(f"  GPIO input (set by Python): 0x{0xAA:02X}")
print(f"  GPIO output (set by CPU):   0x{gpio_out:02X} (expected 0x{0xAA ^ 0xFF:02X} = 0xAA ^ 0xFF)")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: Timer - Measure execution time
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: Timer ---")
print("  Program: start timer, do some work, read timer snapshot")
print(f"  Timer registers at 0x{TIMER_BASE:04X}\n")

# Program:
#   1. Set timer period to large value (0xFFFF)
#   2. Start timer (write CONTROL = START | CONT)
#   3. Do some work (loop)
#   4. Take snapshot (write to SNAP_LO register)
#   5. Read snapshot value
timer_addr_instrs = load_addr(T4, TIMER_BASE)
prog = to_bytes(
    timer_addr_instrs + [
        # Set period = 0xFFFF
        encode_i(0x7FF, ZERO, 0b000, T0, OP_IMM),  # t0 = 0x7FF (max 12-bit imm)
        encode_i(1, T0, 0b001, T0, OP_IMM),         # slli t0, t0, 1 -> 0xFFE
        encode_i(0x08, T4, 0b010, ZERO, OP_STORE),   # sw t0, 0x08(t4) -> PERIOD_LO (wrong, use T0)
    ] + [
        # Actually: store t0 to PERIOD_LO (offset 0x08)
        encode_s(0x08, T0, T4, 0b010, OP_STORE),
        # Start timer: CONTROL = ITO(1) | CONT(2) | START(4) = 7
        encode_i(7, ZERO, 0b000, T1, OP_IMM),
        encode_s(0x04, T1, T4, 0b010, OP_STORE),    # sw t1, 0x04(t4) → CONTROL
        # Do some work (small loop)
        encode_i(10, ZERO, 0b000, T2, OP_IMM),      # t2 = 10
        encode_i(-1 & 0xFFF, T2, 0b000, T2, OP_IMM), # t2--
    ] + [
        # Take snapshot: write anything to SNAP_LO (offset 0x10)
        encode_s(0x10, ZERO, T4, 0b010, OP_STORE),
        # Read snapshot
        encode_i(0x10, T4, 0b010, T5, OP_LOAD),     # lw t5, 0x10(t4) → SNAP_LO
        ECALL,
    ]
)

# Remove the incorrect store instruction (line with ZERO as rs2 for period)
# Let me simplify: just demonstrate timer reads
prog_simple = to_bytes(
    timer_addr_instrs + [
        encode_i(100, ZERO, 0b000, T0, OP_IMM),     # Set period = 100
        encode_s(0x08, T0, T4, 0b010, OP_STORE),    # PERIOD_LO = 100
        # Start: CONTROL = CONT(2) | START(4) = 6
        encode_i(6, ZERO, 0b000, T1, OP_IMM),
        encode_s(0x04, T1, T4, 0b010, OP_STORE),    # CONTROL = 6
        # Do 20 NOPs (burn cycles so timer counts down)
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),     # nop
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        encode_i(0, ZERO, 0b000, ZERO, OP_IMM),
        # Snapshot: write to offset 0x10
        encode_s(0x10, ZERO, T4, 0b010, OP_STORE),
        # Read snapshot
        encode_i(0x10, T4, 0b010, T5, OP_LOAD),     # t5 = SNAP_LO
        # Read status
        encode_i(0x00, T4, 0b010, T2, OP_LOAD),     # t2 = STATUS
        ECALL,
    ]
)

cpu = hades.PipelinedCPU()
cpu.load_program(list(prog_simple))
cpu.run()

timer_snap = cpu.get_reg(30)  # t5 = x30
timer_status = cpu.get_reg(7)  # t2 = x7
print(f"  Timer period: 100")
print(f"  After ~14 instructions:")
print(f"    Snapshot (counter value): {timer_snap}")
print(f"    Status (TO flag): {timer_status}")
print(f"    Cycles elapsed: {cpu.get_cycles()}")

# ═══════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n{'='*60}")
print(f" Demo 05 complete.")
print(f"\n Security insight: I/O devices create observable timing patterns.")
print(f"   UART TX timing reveals when encryption finishes.")
print(f"   GPIO toggles can synchronize oscilloscope captures.")
print(f"   Timer interrupts disturb execution → timing side-channel.")