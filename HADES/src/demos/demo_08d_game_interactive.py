"""
Demo 08c: Interactive Game — assemble guess_game.S and play via keyboard.

=== What this demo shows ===
1. Assembles programs/guess_game.S using the real RISC-V cross-compiler
2. Loads the binary into HADES
3. YOU play the game interactively by typing guesses from the keyboard

=== How to run ===
    make demo-08d

Then type a digit (1-9) and press Enter for each guess.
The secret number is 5. Try to find it!
Type 'q' to quit.

=== Prerequisites ===
Requires: riscv64-unknown-elf-gcc (install: sudo apt install gcc-riscv64-unknown-elf)
"""
import sys, os, subprocess, tempfile, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

PROJECT_ROOT = os.path.join(os.path.dirname(__file__), '..')
ASM_FILE = os.path.join(PROJECT_ROOT, 'programs', 'guess_game.S')
LINK_SCRIPT = os.path.join(PROJECT_ROOT, 'programs', 'link.ld')

# ═══════════════════════════════════════════════════════════════════════════
# Step 1: Check cross-compiler
# ═══════════════════════════════════════════════════════════════════════════

try:
    result = subprocess.run(['riscv64-unknown-elf-gcc', '--version'],
                           capture_output=True, text=True)
    if result.returncode != 0:
        raise FileNotFoundError
except FileNotFoundError:
    print("riscv64-unknown-elf-gcc not found!")
    print("Install: sudo apt install gcc-riscv64-unknown-elf")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════════
# Step 2: Assemble .S -> .elf -> .bin
# ═══════════════════════════════════════════════════════════════════════════

with tempfile.TemporaryDirectory() as tmpdir:
    elf_file = os.path.join(tmpdir, 'guess_game.elf')
    bin_file = os.path.join(tmpdir, 'guess_game.bin')

    # Assemble + link
    result = subprocess.run([
        'riscv64-unknown-elf-gcc',
        '-march=rv32i', '-mabi=ilp32', '-nostdlib',
        '-T', LINK_SCRIPT,
        '-o', elf_file, ASM_FILE
    ], capture_output=True, text=True)

    if result.returncode != 0:
        print(f"Assembly failed:\n{result.stderr}")
        sys.exit(1)

    # Convert to raw binary
    result = subprocess.run([
        'riscv64-unknown-elf-objcopy', '-O', 'binary',
        elf_file, bin_file
    ], capture_output=True, text=True)

    if result.returncode != 0:
        print(f"objcopy failed:\n{result.stderr}")
        sys.exit(1)

    with open(bin_file, 'rb') as f:
        binary = list(f.read())

# ═══════════════════════════════════════════════════════════════════════════
# Step 3: Interactive game
# ═══════════════════════════════════════════════════════════════════════════

print("=" * 60)
print("DEMO 08c: Interactive 'Guess the Number'")
print(f"  (assembled from programs/guess_game.S, {len(binary)} bytes)")
print("=" * 60)
print()
print("  The CPU has picked a secret number between 1 and 9.")
print("  Type your guess (a single digit) and press Enter.")
print("  Type 'q' to quit.")
print()

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.load_program(binary, 0x1000)

# Run enough cycles to display title (CPU spins waiting for UART input)
cpu.uart_send([])
cpu.run(0)
time.sleep(0.01)  # let CPU thread write title to VGA

# Show title
print("  ┌────────────────────┐")
title = cpu.vga_get_char_row(0).rstrip()
print(f"  │ {title:<18} │")
print("  └────────────────────┘")
print()

guess_num = 0
won = False

while not won:
    try:
        user_input = input("  Your guess (1-9): ").strip()
    except (EOFError, KeyboardInterrupt):
        print("\n  Quit.")
        break

    if user_input == 'q':
        print("  Quit.")
        break

    if len(user_input) != 1 or user_input not in '123456789':
        print("  Please enter a single digit 1-9.")
        continue

    guess_num += 1

    # Send guess to CPU via UART
    cpu.uart_send([ord(user_input)])

    # Wait for CPU to finish writing response to VGA
    for _ in range(50):
        time.sleep(0.005)
        row_text = cpu.vga_get_char_row(guess_num).rstrip()
        if len(row_text) >= 5:  # e.g. ">5 LOW" is at least 5 chars
            break

    print(f"  PipelinedCPU says: {row_text}")

    # Check if won
    tx = cpu.uart_recv()
    if ord('W') in tx:
        won = True

print()
if won:
    print(f"  You won in {guess_num} guesses!")
else:
    print(f"  Game ended after {guess_num} guesses.")

print()
print("  Final VGA Display:")
print("  ┌────────────────────┐")
for row in range(guess_num + 1):
    line = cpu.vga_get_char_row(row).rstrip()
    if line:
        print(f"  │ {line:<18} │")
print("  └────────────────────┘")
print()
print(f"  Total PipelinedCPU cycles: {cpu.get_cycles()}")