"""
Demo 08b: Run the assembly game using the RISC-V cross-compiler.

=== What this demo shows ===
Same game as demo_08, but this time we:
1. Assemble programs/guess_game.S using riscv64-unknown-elf-gcc
2. Convert to raw binary with objcopy
3. Load the binary into the simulator
4. Play the game via UART + read VGA output

This proves the real toolchain produces working code for HADES.

=== Prerequisites ===
Requires: riscv64-unknown-elf-gcc (install: sudo apt install gcc-riscv64-unknown-elf)

=== How to run ===
    make demo-08b
"""
import sys, os, subprocess, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 08b: Assemble & Run guess_game.S (cross-compiler)")
print("=" * 60)

PROJECT_ROOT = os.path.join(os.path.dirname(__file__), '..')
ASM_FILE = os.path.join(PROJECT_ROOT, 'programs', 'guess_game.S')
LINK_SCRIPT = os.path.join(PROJECT_ROOT, 'programs', 'link.ld')

# ═══════════════════════════════════════════════════════════════════════════
# Step 1: Check cross-compiler is available
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Step 1: Check toolchain ---")

try:
    result = subprocess.run(['riscv64-unknown-elf-gcc', '--version'],
                           capture_output=True, text=True)
    if result.returncode != 0:
        raise FileNotFoundError
    print(f"   riscv64-unknown-elf-gcc found")
    print(f"     {result.stdout.splitlines()[0]}")
except FileNotFoundError:
    print("   riscv64-unknown-elf-gcc not found!")
    print("  Install: sudo apt install gcc-riscv64-unknown-elf")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════════
# Step 2: Assemble .S → .elf → .bin
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Step 2: Assemble programs/guess_game.S ---")

with tempfile.TemporaryDirectory() as tmpdir:
    elf_file = os.path.join(tmpdir, 'guess_game.elf')
    bin_file = os.path.join(tmpdir, 'guess_game.bin')

    # Assemble + link
    cmd_gcc = [
        'riscv64-unknown-elf-gcc',
        '-march=rv32i', '-mabi=ilp32',
        '-nostdlib',
        '-T', LINK_SCRIPT,
        '-o', elf_file,
        ASM_FILE
    ]
    print(f"  Command: {' '.join(cmd_gcc)}")
    result = subprocess.run(cmd_gcc, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  Assembly failed:\n{result.stderr}")
        sys.exit(1)
    print(f"  Assembled -> {os.path.basename(elf_file)}")

    # Convert to raw binary
    cmd_objcopy = [
        'riscv64-unknown-elf-objcopy',
        '-O', 'binary',
        elf_file, bin_file
    ]
    result = subprocess.run(cmd_objcopy, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  objcopy failed:\n{result.stderr}")
        sys.exit(1)

    bin_size = os.path.getsize(bin_file)
    print(f"  Converted -> {os.path.basename(bin_file)} ({bin_size} bytes)")

    # Read binary
    with open(bin_file, 'rb') as f:
        binary = list(f.read())

# ═══════════════════════════════════════════════════════════════════════════
# Step 3: Load into simulator and play the game
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n--- Step 3: Run game on HADES ({len(binary)} bytes loaded at 0x1000) ---")
print(f"  Sending guesses via UART: '3', '7', '5' (binary search for secret=5)\n")

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.uart_send([ord('3'), ord('7'), ord('5')])
cpu.load_program(binary, 0x1000)
cpu.run(100000)

# ═══════════════════════════════════════════════════════════════════════════
# Step 4: Read VGA output
# ═══════════════════════════════════════════════════════════════════════════

print(f"  VGA Display:")
print(f"  ┌{'─'*20}┐")
for row in range(6):
    line = cpu.vga_get_char_row(row).rstrip()
    if line:
        print(f"  │ {line:<18} │")
print(f"  └{'─'*20}┘")

# Check win
tx_output = cpu.uart_recv()
won = ord('W') in tx_output

print(f"\n  UART TX: {[chr(b) for b in tx_output]}")
print(f"  Result:  {'WON!' if won else 'Not won'}")
print(f"  Cycles:  {cpu.get_cycles()}")

# ═══════════════════════════════════════════════════════════════════════════
# Step 5: Play again with different guesses
# ═══════════════════════════════════════════════════════════════════════════

print(f"\n--- Step 4: Second playthrough (guesses: 2, 8, 5) ---")

cpu2 = hades.PipelinedCPU()
cpu2.set_io_enabled(True)
cpu2.uart_send([ord('2'), ord('8'), ord('5')])
cpu2.load_program(binary, 0x1000)
cpu2.run(100000)

print(f"  VGA Display:")
print(f"  ┌{'─'*20}┐")
for row in range(5):
    line = cpu2.vga_get_char_row(row).rstrip()
    if line:
        print(f"  │ {line:<18} │")
print(f"  └{'─'*20}┘")

tx2 = cpu2.uart_recv()
won2 = ord('W') in tx2
print(f"  Result: {'WON!' if won2 else 'Not won'}")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f"Demo 08b complete.")
print(f"\n  This proves the REAL RISC-V toolchain (gcc + objcopy) produces")
print(f"   binaries that run correctly on the HADES simulator.")
print(f"   The same .S file could run on real RISC-V hardware!")