"""
HADES Peripherals — Host-side I/O drivers.

This module provides the "other end" of the simulated I/O devices:
- TerminalInput: reads keyboard input, converts to UART bytes
- TerminalDisplay: reads VGA char+color buffers, renders with ANSI colors

These drivers bridge between the real world (your keyboard/terminal)
and the simulated hardware (UART FIFO, VGA buffer).
"""
import sys

# ═══════════════════════════════════════════════════════════════════════════
# VGA Color Constants (must match VGA COLOR_ATTR values in hardware)
# ═══════════════════════════════════════════════════════════════════════════

VGA_COLOR_DEFAULT = 0
VGA_COLOR_RED = 1
VGA_COLOR_GREEN = 2
VGA_COLOR_BLUE = 3
VGA_COLOR_YELLOW = 4
VGA_COLOR_CYAN = 5
VGA_COLOR_MAGENTA = 6

# ANSI escape codes indexed by VGA color attribute
ANSI_COLORS = {
    0: '\033[0m',    # default (white)
    1: '\033[91m',   # red
    2: '\033[92m',   # green
    3: '\033[94m',   # blue
    4: '\033[93m',   # yellow
    5: '\033[96m',   # cyan
    6: '\033[95m',   # magenta
}
ANSI_RESET = '\033[0m'

VGA_COLS = 80
VGA_ROWS = 60


# ═══════════════════════════════════════════════════════════════════════════
# TerminalDisplay: reads VGA buffers and renders to terminal
# ═══════════════════════════════════════════════════════════════════════════

class TerminalDisplay:
    """Renders VGA character + color buffer to the host terminal using ANSI codes."""

    def __init__(self, visible_rows=24):
        self.visible_rows = visible_rows

    def render(self, cpu, status_line=""):
        """Clear terminal and redraw VGA content with colors."""
        chars = cpu.vga_get_char_buffer()
        colors = cpu.vga_get_color_buffer()

        sys.stdout.write('\033[H\033[J')  # clear screen
        sys.stdout.write('┌' + '─' * VGA_COLS + '┐\n')

        for row in range(self.visible_rows):
            sys.stdout.write('│')
            for col in range(VGA_COLS):
                idx = row * VGA_COLS + col
                ch = chars[idx]
                color_id = colors[idx]
                c = chr(ch) if 32 <= ch < 127 else ' '
                ansi = ANSI_COLORS.get(color_id, ANSI_RESET)
                sys.stdout.write(ansi + c)
            sys.stdout.write(ANSI_RESET + '│\n')

        sys.stdout.write('└' + '─' * VGA_COLS + '┘\n')
        if status_line:
            sys.stdout.write(status_line + '\n')
        sys.stdout.write('> ')
        sys.stdout.flush()

    def render_simple(self, cpu, max_rows=None):
        """Render without clearing screen (for non-interactive use)."""
        if max_rows is None:
            max_rows = self.visible_rows

        chars = cpu.vga_get_char_buffer()
        colors = cpu.vga_get_color_buffer()

        print('┌' + '─' * VGA_COLS + '┐')
        for row in range(max_rows):
            line = ''
            for col in range(VGA_COLS):
                idx = row * VGA_COLS + col
                ch = chars[idx]
                color_id = colors[idx]
                c = chr(ch) if 32 <= ch < 127 else ' '
                ansi = ANSI_COLORS.get(color_id, ANSI_RESET)
                line += ansi + c
            # Only print rows that have content
            raw = ''.join(chr(chars[row * VGA_COLS + col])
                         if 32 <= chars[row * VGA_COLS + col] < 127 else ' '
                         for col in range(VGA_COLS))
            if raw.rstrip():
                print('│' + line + ANSI_RESET + '│')
        print('└' + '─' * VGA_COLS + '┘')