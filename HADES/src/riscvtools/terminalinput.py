"""
HADES Peripherals — Host-side I/O drivers.

This module provides the "other end" of the simulated I/O devices:
- TerminalDisplay: reads VGA char+color buffers, renders with ANSI colors

These drivers bridge between the real world (your keyboard/terminal)
and the simulated hardware (UART FIFO, VGA buffer).
"""

# ═══════════════════════════════════════════════════════════════════════════
# TerminalInput: reads keyboard and converts to UART bytes
# ═══════════════════════════════════════════════════════════════════════════

class TerminalInput:
    """Reads keyboard input and sends to CPU via UART."""

    def __init__(self, cpu):
        self.cpu = cpu

    def send_line(self, text):
        """Send a line of text + newline to UART."""
        chars = []
        for ch in text:
            if 32 <= ord(ch) < 127:
                chars.append(ord(ch))
        chars.append(0x0A)  # newline
        self.cpu.uart_send(chars)

    def send_chars(self, text):
        """Send characters without newline."""
        chars = [ord(ch) for ch in text if 32 <= ord(ch) < 127]
        self.cpu.uart_send(chars)

    def send_raw(self, bytes_list):
        """Send raw byte values."""
        self.cpu.uart_send(bytes_list)

    def read_line(self, prompt="> "):
        """Read a line from keyboard. Returns None on EOF/Ctrl+C."""
        try:
            return input(prompt)
        except (EOFError, KeyboardInterrupt):
            return None