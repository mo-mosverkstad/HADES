# Demo 11 - Echo Terminal (Real-Time Character Display)

This demo simulates a real serial terminal: characters typed on the keyboard are sent to the CPU via UART, the CPU echoes them back and displays them on the VGA screen. The Python host refreshes the display after each input.

---

## Prerequisites

- WSL2 with Ubuntu 22.04+
- Python venv activated: `source ~/projects/hwsec/bin/activate`
- Engine built: `make engine`
- Cross-compiler: `sudo apt install gcc-riscv64-unknown-elf`

---

## Quick Run (One Command)

```bash
make demo-13
```

---

## How It Works

```
Your keyboard
    │
    │  You type "Hello" + Enter
    ▼
Python input()
    │
    │  Converts to bytes: [72, 101, 108, 108, 111, 10]
    ▼
cpu.uart_send(bytes)
    │
    │  Bytes go into UART RX FIFO
    ▼
CPU echo_terminal.S runs:
    │  - Reads each byte from UART
    │  - Echoes byte back to UART TX
    │  - Writes character to VGA at cursor position
    │  - Advances cursor (newline = next row)
    ▼
Python reads VGA:
    │  cpu.vga_get_char_row(0..23)
    │  Clears terminal, redraws display
    ▼
Your screen shows:
    ┌────────────────────────────────────────┐
    │Hello                                   │
    │                                        │
    └────────────────────────────────────────┘
```

---

## Controls

| Key | Action |
|-----|--------|
| Any printable character | Displayed on VGA at cursor |
| Enter | Move to next line |
| Type 'quit' | Exit the demo |
| Ctrl+C | Exit the demo |

---

## Assembly Source: `programs/echo_terminal.S`

Key features:
- Spins on UART read (waits for RVALID bit)
- Echoes every received byte back to UART TX
- Writes character to VGA CHAR_WRITE register
- Handles newline (0x0A): cursor to start of next row
- Handles backspace (0x08): erase previous character
- Handles Ctrl+C (0x03): halt CPU
- Wraps at row 60 back to row 0

---

## Display Refresh

Python clears and redraws the terminal after each input using ANSI escape codes:
```python
sys.stdout.write('\033[H\033[J')  # clear screen
for row in range(24):
    line = cpu.vga_get_char_row(row)
    print('│' + line + '│')
```

This gives a real-time display effect - old output is replaced, not accumulated.

---

## What This Demo Proves

- Full echo system: keyboard -> UART -> CPU -> VGA -> screen
- CPU handles character-by-character I/O in a loop
- VGA cursor management (advance, newline, backspace)
- UART bidirectional: input AND echo output
- Real assembly file assembled by gcc toolchain
- Display refresh (clear + redraw, no stale output)