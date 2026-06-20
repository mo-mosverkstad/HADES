"""
Demo 07: VGA Display — pixel buffer and character buffer output.

=== What this demo shows ===
1. Character mode: CPU writes ASCII text to VGA, Python reads it back
2. Pixel mode: CPU writes colored pixels, Python reads framebuffer
3. How VGA output makes CPU state visible (debugging + side-channel)

=== Background: VGA in Embedded Systems ===
The VGA controller provides visual output from the CPU:
- Character buffer (80*60): fast text display, like a terminal
- Pixel buffer (320*240, RGB565): graphics, plots, visualizations

In HADES, the VGA is memory-mapped at 0xF080:
  0xF080 + 0x00: CONTROL (mode, enable)
  0xF080 + 0x08: CURSOR_X
  0xF080 + 0x0C: CURSOR_Y
  0xF080 + 0x10: PIXEL_ADDR
  0xF080 + 0x14: PIXEL_DATA (write pixel, auto-increment)
  0xF080 + 0x18: CHAR_WRITE (write char at cursor, auto-advance)

=== How to run ===
    make demo-07
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 07: VGA Display (Character + Pixel Buffer)")
print("=" * 60)

VGA_BASE = 0xF080

# Helper to load VGA base address into a register
def load_vga_addr(rd):
    return [
        encode_u(0x0000F000, rd, OP_LUI),
        encode_i(0x080, rd, 0b000, rd, OP_IMM),
    ]

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: Character Mode — CPU writes "HADES" to VGA character buffer
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Character Mode ---")
print("  CPU writes 'HADES' to VGA character buffer at cursor (0,0)")
print("  Python reads back the character buffer\n")

# Program:
#   1. Set CONTROL = mode_char(1) | enable(2) = 3
#   2. Set CURSOR_X = 0, CURSOR_Y = 0
#   3. Write 'H','A','D','E','S' to CHAR_WRITE register (auto-advances cursor)

chars_to_write = [ord('H'), ord('A'), ord('D'), ord('E'), ord('S')]

instrs = load_vga_addr(T4)  # t4 = 0xF080 (VGA base)
# Set CONTROL = 3 (char mode + enable)
instrs += [
    encode_i(3, ZERO, 0b000, T0, OP_IMM),
    encode_s(0x00, T0, T4, 0b010, OP_STORE),  # CONTROL = 3
]
# Set CURSOR_X = 0
instrs += [
    encode_s(0x08, ZERO, T4, 0b010, OP_STORE),  # CURSOR_X = 0
]
# Set CURSOR_Y = 0
instrs += [
    encode_s(0x0C, ZERO, T4, 0b010, OP_STORE),  # CURSOR_Y = 0
]
# Write each character
for ch in chars_to_write:
    instrs += [
        encode_i(ch, ZERO, 0b000, T0, OP_IMM),
        encode_s(0x18, T0, T4, 0b010, OP_STORE),  # CHAR_WRITE = ch
    ]
instrs.append(ECALL)

prog = to_bytes(instrs)
cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.load_program(list(prog))
cpu.run()

# Read back character buffer row 0
row0 = cpu.vga_get_char_row(0)
print(f"  VGA char row 0: '{row0[:10].rstrip()}'")
print(f"  Expected:        'HADES'")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Pixel Mode — CPU writes colored pixels
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 2: Pixel Mode ---")
print("  CPU writes 5 pixels with different RGB565 colors")
print("  Python reads framebuffer and displays values\n")

# RGB565 format: RRRRRGGGGGGBBBBB (5-6-5 bits)
# Red   = 0xF800 (11111_000000_00000)
# Green = 0x07E0 (00000_111111_00000)
# Blue  = 0x001F (00000_000000_11111)
# White = 0xFFFF
# Black = 0x0000

colors = [0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000]
color_names = ["Red", "Green", "Blue", "White", "Black"]

instrs = load_vga_addr(T4)
# Set CONTROL = enable(2) only (pixel mode, bit 0 = 0)
instrs += [
    encode_i(2, ZERO, 0b000, T0, OP_IMM),
    encode_s(0x00, T0, T4, 0b010, OP_STORE),
]
# Set PIXEL_ADDR = 0 (start at top-left)
instrs += [
    encode_s(0x10, ZERO, T4, 0b010, OP_STORE),
]
# Write 5 pixels (PIXEL_DATA auto-increments address)
for color in colors:
    instrs += [
        encode_i(color & 0xFFF, ZERO, 0b000, T0, OP_IMM),  # low 12 bits
    ]
    if color > 0xFFF:
        # Need LUI for upper bits
        instrs = instrs[:-1]  # remove the addi
        instrs += [
            encode_u((color << 16) & 0xFFFFF000, T0, OP_LUI),
            encode_i(color & 0xFFF, T0, 0b000, T0, OP_IMM),
        ]
    instrs += [
        encode_s(0x14, T0, T4, 0b010, OP_STORE),  # PIXEL_DATA = color
    ]
instrs.append(ECALL)

prog = to_bytes(instrs)
cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.load_program(list(prog))
cpu.run()

# Read framebuffer
fb = cpu.vga_get_framebuffer()
print(f"  Framebuffer pixels 0-4:")
for i in range(5):
    actual = fb[i]
    expected = colors[i] & 0xFFF  # we only wrote low 12 bits for simplicity
    print(f"    Pixel {i}: 0x{actual:04X} ({color_names[i]})")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: Reading VGA state from Python (debugging use)
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: VGA as Debug Output ---")
print("  In real embedded development, VGA/LCD shows program state")
print("  In HADES, Python reads the character buffer for verification\n")

# Write a multi-line message
instrs = load_vga_addr(T4)
instrs += [
    encode_i(3, ZERO, 0b000, T0, OP_IMM),
    encode_s(0x00, T0, T4, 0b010, OP_STORE),  # char mode + enable
    encode_s(0x08, ZERO, T4, 0b010, OP_STORE),  # cursor_x = 0
    encode_s(0x0C, ZERO, T4, 0b010, OP_STORE),  # cursor_y = 0
]
# Write "OK" on row 0
for ch in [ord('O'), ord('K')]:
    instrs += [
        encode_i(ch, ZERO, 0b000, T0, OP_IMM),
        encode_s(0x18, T0, T4, 0b010, OP_STORE),
    ]
# Move cursor to row 1
instrs += [
    encode_s(0x08, ZERO, T4, 0b010, OP_STORE),  # cursor_x = 0
    encode_i(1, ZERO, 0b000, T1, OP_IMM),
    encode_s(0x0C, T1, T4, 0b010, OP_STORE),    # cursor_y = 1
]
# Write "42" on row 1
for ch in [ord('4'), ord('2')]:
    instrs += [
        encode_i(ch, ZERO, 0b000, T0, OP_IMM),
        encode_s(0x18, T0, T4, 0b010, OP_STORE),
    ]
instrs.append(ECALL)

prog = to_bytes(instrs)
cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.load_program(list(prog))
cpu.run()

print(f"  VGA row 0: '{cpu.vga_get_char_row(0)[:10].rstrip()}'")
print(f"  VGA row 1: '{cpu.vga_get_char_row(1)[:10].rstrip()}'")
print(f"  (CPU wrote 'OK' on line 0, '42' on line 1)")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f"Demo 07 complete.")