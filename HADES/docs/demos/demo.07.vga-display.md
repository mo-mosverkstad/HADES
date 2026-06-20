# Demo 07 - VGA Display (Pixel + Character Buffer)

This demo shows the VGA display device outputting text and pixels from CPU programs, readable from Python.

---

## Prerequisites

- Phase 6 completed (multi-core working)
- Engine built (`make engine`)

---

## Key Concepts

- **Character buffer**: 80*60 ASCII grid, cursor auto-advances on write
- **Pixel buffer**: 320*240 RGB565, address auto-increments on write
- **Memory-mapped**: CPU writes to VGA registers like normal memory stores
- **Python readback**: host can inspect display content for verification/debugging

---

## VGA Register Map (base 0xF080)

| Offset | Register | Function |
|--------|----------|----------|
| 0x00 | CONTROL | [0] mode (0=pixel, 1=char), [1] enable |
| 0x08 | CURSOR_X | Character cursor column (0-79) |
| 0x0C | CURSOR_Y | Character cursor row (0-59) |
| 0x10 | PIXEL_ADDR | Pixel write address (auto-increment) |
| 0x14 | PIXEL_DATA | Write RGB565 pixel at PIXEL_ADDR |
| 0x18 | CHAR_WRITE | Write ASCII char at cursor (auto-advance) |

---

## Quick Run (One Command)

```bash
make demo-09
```

This runs `demos/demo_09_vga.py` which demonstrates character and pixel output.

---

## Expected Output

```
Part 1: VGA char row 0: 'HADES'
Part 2: Pixels: Red(0xF800), Green(0x07E0), Blue(0x001F), White(0xFFFF), Black(0x0000)
Part 3: Row 0: 'OK', Row 1: '42'
```

---

## What This Demo Proves

- Character buffer correctly stores and retrieves ASCII text
- Pixel buffer correctly stores RGB565 color values
- Auto-increment works (cursor and pixel address advance)
- Python can read back VGA state for verification
- VGA serves as debug output channel for CPU programs