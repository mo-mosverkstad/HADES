"""
Demo 11: Echo Terminal with Color - using layer2_peripherals drivers.

=== What this demo shows ===
1. Assembles programs/echo_terminal.S using AssemblyLoader
2. You type characters -> TerminalInput sends to CPU via UART
3. CPU parses '$' commands, sets VGA color, writes characters
4. TerminalDisplay reads VGA char+color buffers, renders with ANSI colors

=== Color Commands (handled by CPU assembly) ===
- $r -> red    $g -> green    $b -> blue
- $y -> yellow $c -> cyan     $m -> magenta
- $- -> default (white)      $q -> quit

=== How to run ===
    make demo-11
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

PROJECT_ROOT = os.path.join(os.path.dirname(__file__), '..')
ASM_FILE = os.path.join(PROJECT_ROOT, 'programs', 'echo_terminal.S')

# ═══════════════════════════════════════════════════════════════════════════
# Step 1: Assemble using AssemblyLoader
# ═══════════════════════════════════════════════════════════════════════════

loader = AssemblyLoader()
if not loader.check_toolchain():
    print("Error: riscv64-unknown-elf-gcc not found!")
    print("Install: sudo apt install gcc-riscv64-unknown-elf")
    sys.exit(1)

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
bin_size = loader.load(cpu, ASM_FILE)
if bin_size == 0:
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════════
# Step 2: Setup peripherals
# ═══════════════════════════════════════════════════════════════════════════

display = TerminalDisplay(visible_rows=24)
terminal = TerminalInput(cpu)

# ═══════════════════════════════════════════════════════════════════════════
# Step 3: Interactive loop
# ═══════════════════════════════════════════════════════════════════════════

print("=" * 60)
print("DEMO 11: Echo Terminal with Color")
print(f"  (assembled from programs/echo_terminal.S, {bin_size} bytes)")
print("=" * 60)
print()
print("  Type text -> displayed on VGA with color.")
print("  Color commands (processed by CPU assembly):")
print("    $r=red  $g=green  $b=blue  $y=yellow  $c=cyan  $m=magenta")
print("    $- = default    $q = quit")
print()
print("  Press Enter to start...")
input()

# Initial CPU run (sets up VGA)
cpu.uart_send([])
cpu.run(5000)

status = f'[Cycles: {cpu.get_cycles()}] Colors: $r $g $b $y $c $m $- | $q=quit'
display.render(cpu, status)

running = True
while running:
    user_input = terminal.read_line("")
    if user_input is None:
        break

    # Send to CPU via UART
    terminal.send_line(user_input)

    # Run CPU to process input
    cpu.run((len(user_input) + 1) * 500)

    # Check if CPU halted ($q)
    if cpu.is_halted():
        running = False

    # Refresh display from VGA
    status = f'[Cycles: {cpu.get_cycles()}] Colors: $r $g $b $y $c $m $- | $q=quit'
    display.render(cpu, status)

# Clean exit
sys.stdout.write('\033[H\033[J')
print("Session ended.")
print(f"Total CPU cycles: {cpu.get_cycles()}")