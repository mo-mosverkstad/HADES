"""
Demo 08: Interactive Game — "Guess the Number" running on HADES CPU.

=== What this demo shows ===
1. A complete program (game) running on the simulated RISC-V CPU
2. Input via UART (simulates keyboard — Python feeds guesses)
3. Output via VGA character buffer (simulates screen display)
4. Full I/O loop: read input → process → display output → repeat

=== The Game ===
The CPU picks a secret number (1-9). The player sends guesses via UART.
The VGA displays each guess with feedback: "LOW", "HIGH", or "WIN!"

In this demo, Python plays the game automatically using binary search,
demonstrating that the CPU correctly processes I/O in a loop.

=== Architecture ===
    Python (host)                    HADES CPU
    ─────────────                    ─────────
    uart_send('3')  ──────────►  UART RX FIFO
                                     │
                                 CPU reads UART
                                 compares with secret (5)
                                 writes "LOW" to VGA
                                     │
    vga_get_char_row() ◄──────── VGA char buffer
    uart_recv()        ◄──────── UART TX (on win)

=== How to run ===
    make demo-08

=== Note ===
This demo uses hand-encoded machine code (no cross-compiler needed).
The assembly source is in programs/guess_game.S for reference.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 08: 'Guess the Number' Game on HADES CPU")
print("=" * 60)

# ─── Generate game machine code ──────────────────────────────────────────
# This encodes the same logic as programs/guess_game.S

UART_BASE = 0xF020
VGA_BASE = 0xF080

def LI(rd, imm):
    return encode_i(imm & 0xFFF, ZERO, 0b000, rd, OP_IMM)

def build_game_program():
    """Build the guess game as RV32I machine code."""
    code = []

    # Setup UART address in t4
    code.append(encode_u(0x0000F000, T4, OP_LUI))       # t4 = 0xF000
    code.append(encode_i(0x020, T4, 0b000, T4, OP_IMM)) # t4 = 0xF020

    # Setup VGA address in t5
    code.append(encode_u(0x0000F000, T5, OP_LUI))       # t5 = 0xF000
    code.append(encode_i(0x080, T5, 0b000, T5, OP_IMM)) # t5 = 0xF080

    # VGA CONTROL = 3 (char mode + enable)
    code.append(LI(T0, 3))
    code.append(encode_s(0x00, T0, T5, 0b010, OP_STORE))

    # Cursor to (0,0)
    code.append(encode_s(0x08, ZERO, T5, 0b010, OP_STORE))
    code.append(encode_s(0x0C, ZERO, T5, 0b010, OP_STORE))

    # Print "GUESS" on row 0
    for ch in "GUESS 1-9":
        code.append(LI(T0, ord(ch)))
        code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))

    # Secret number = 5
    code.append(LI(S0, 5))

    # Output row counter = 1
    code.append(LI(S1, 1))

    # ─── game_loop ───
    game_loop_pc = len(code)

    # Read UART: lw t0, 0(t4)
    code.append(encode_i(0, T4, 0b010, T0, OP_LOAD))

    # Check RVALID (bit 15): andi t1, t0, 0x8000 — but 0x8000 > 12-bit imm
    # Use: srli t1, t0, 15; andi t1, t1, 1
    code.append(encode_i(15, T0, 0b101, T1, OP_IMM))  # srli t1, t0, 15
    code.append(encode_i(1, T1, 0b111, T1, OP_IMM))   # andi t1, t1, 1

    # beqz t1, game_loop (spin if no data)
    spin_offset = (game_loop_pc - len(code)) * 4
    code.append(encode_b(spin_offset & 0x1FFF, ZERO, T1, 0b000, OP_BRANCH))

    # Extract byte: andi t2, t0, 0xFF
    code.append(encode_i(0xFF, T0, 0b111, T2, OP_IMM))

    # Set cursor: x=0, y=s1
    code.append(encode_s(0x08, ZERO, T5, 0b010, OP_STORE))
    code.append(encode_s(0x0C, S1, T5, 0b010, OP_STORE))

    # Display ">" + guess
    code.append(LI(T0, ord('>')))
    code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))
    code.append(encode_s(0x18, T2, T5, 0b010, OP_STORE))
    code.append(LI(T0, ord(' ')))
    code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))

    # Convert ASCII to number: t3 = t2 - '0'
    code.append(encode_i(-48 & 0xFFF, T2, 0b000, T3, OP_IMM))

    # Compare: beq t3, s0, win
    win_offset_placeholder = len(code)
    code.append(0)  # placeholder for beq

    # blt t3, s0, too_low
    too_low_placeholder = len(code)
    code.append(0)  # placeholder for blt

    # ─── TOO HIGH ───
    for ch in "HIGH":
        code.append(LI(T0, ord(ch)))
        code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))

    # j next_round
    next_round_jump_placeholder = len(code)
    code.append(0)  # placeholder

    # ─── too_low ───
    too_low_pc = len(code)
    for ch in "LOW":
        code.append(LI(T0, ord(ch)))
        code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))

    # j next_round
    next_round_jump2_placeholder = len(code)
    code.append(0)  # placeholder

    # ─── win ───
    win_pc = len(code)
    for ch in "WIN!":
        code.append(LI(T0, ord(ch)))
        code.append(encode_s(0x18, T0, T5, 0b010, OP_STORE))

    # Echo 'W' to UART TX
    code.append(LI(T0, ord('W')))
    code.append(encode_s(0, T0, T4, 0b010, OP_STORE))

    # ecall (halt)
    code.append(ECALL)

    # ─── next_round ───
    next_round_pc = len(code)
    code.append(encode_i(1, S1, 0b000, S1, OP_IMM))  # s1++

    # j game_loop
    loop_back_offset = (game_loop_pc - len(code)) * 4
    code.append(encode_j(loop_back_offset & 0x1FFFFF, ZERO, OP_JAL))

    # ─── Fix placeholders ───
    # beq t3, s0, win
    win_offset = (win_pc - win_offset_placeholder) * 4
    code[win_offset_placeholder] = encode_b(win_offset & 0x1FFF, S0, T3, 0b000, OP_BRANCH)

    # blt t3, s0, too_low
    too_low_offset = (too_low_pc - too_low_placeholder) * 4
    code[too_low_placeholder] = encode_b(too_low_offset & 0x1FFF, S0, T3, 0b100, OP_BRANCH)

    # j next_round (from HIGH)
    nr_offset1 = (next_round_pc - next_round_jump_placeholder) * 4
    code[next_round_jump_placeholder] = encode_j(nr_offset1 & 0x1FFFFF, ZERO, OP_JAL)

    # j next_round (from LOW)
    nr_offset2 = (next_round_pc - next_round_jump2_placeholder) * 4
    code[next_round_jump2_placeholder] = encode_j(nr_offset2 & 0x1FFFFF, ZERO, OP_JAL)

    return code

# ═══════════════════════════════════════════════════════════════════════════
# Build and run the game
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Building game program ---")
game_code = build_game_program()
game_binary = to_bytes(game_code)
print(f"  Program: {len(game_code)} instructions, {len(game_binary)} bytes")

print("\n--- Playing the game (Python as player, binary search) ---")
print(f"  Secret number: 5 (hardcoded in program)")
print(f"  Strategy: binary search (3, 7, 5)\n")

# Feed guesses via UART: '3', '7', '5' (binary search for 5)
guesses = [ord('3'), ord('7'), ord('5')]

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.uart_send(guesses)
cpu.load_program(list(game_binary))
cpu.run(50000)

# Read VGA output
print(f"  VGA Display:")
print(f"  ┌{'─'*20}┐")
for row in range(5):
    line = cpu.vga_get_char_row(row).rstrip()
    if line:
        print(f"  │ {line:<18} │")
print(f"  └{'─'*20}┘")

# Check UART TX for win signal
tx_output = cpu.uart_recv()
won = ord('W') in tx_output

print(f"\n  UART TX output: {[chr(b) for b in tx_output]}")
print(f"  Game result: {'WON!' if won else 'Not won yet'}")
print(f"  Cycles: {cpu.get_cycles()}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Play again with different guesses
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Playing again (guesses: 1, 9, 4, 6, 5) ---")

guesses2 = [ord('1'), ord('9'), ord('4'), ord('6'), ord('5')]

cpu2 = hades.PipelinedCPU()
cpu2.set_io_enabled(True)
cpu2.uart_send(guesses2)
cpu2.load_program(list(game_binary))
cpu2.run(100000)

print(f"  VGA Display:")
print(f"  ┌{'─'*20}┐")
for row in range(7):
    line = cpu2.vga_get_char_row(row).rstrip()
    if line:
        print(f"  │ {line:<18} │")
print(f"  └{'─'*20}┘")

tx2 = cpu2.uart_recv()
won2 = ord('W') in tx2
print(f"\n  Game result: {'WON!' if won2 else 'Not won'}")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f" Demo 08 complete.")
print(f"\n This demonstrates a COMPLETE embedded system:")
print(f"   - CPU executes game logic (branches, comparisons, loops)")
print(f"   - UART provides input (keyboard simulation)")
print(f"   - VGA provides output (screen simulation)")
print(f"   - The same architecture runs crypto, games, or any program!")