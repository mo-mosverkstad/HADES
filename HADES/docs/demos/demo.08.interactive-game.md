# Demo 08 — Interactive Game (Full System Integration)

This demo runs a complete "Guess the Number" game on the HADES CPU, using UART for input and VGA for display — proving the simulator is a fully functional embedded system.

---

## Prerequisites

- WSL2 with Ubuntu 22.04+
- Python venv activated: `source ~/projects/hwsec/bin/activate`
- Engine built: `make engine`
---

## The Game

- CPU picks a secret number (1-9, hardcoded as 5)
- Player sends ASCII digit guesses via UART
- VGA displays each guess with feedback: "LOW", "HIGH", or "WIN!"
- On win: CPU sends 'W' via UART TX and halts

---

## Quick Run (One Command)

```bash
make demo-08
```

This runs `demos/demo_08_game.py` which plays the game automatically using binary search.

---

## Expected Output

```
VGA Display:
┌────────────────────┐
│ GUESS 1-9          │
│ >3 LOW             │
│ >7 HIGH            │
│ >5 WIN!            │
└────────────────────┘

UART TX output: ['W']
Game result: WON!
```

---

## Assembly Source

The game logic is in `programs/guess_game.S` (also encoded programmatically in the demo). Key structure:

```asm
game_loop:
    lw   t0, 0(uart)       # read UART (blocks until data)
    # ... compare with secret ...
    beq  guess, secret, win
    blt  guess, secret, too_low
    # display "HIGH" on VGA
    j    next_round
too_low:
    # display "LOW" on VGA
    j    next_round
win:
    # display "WIN!" on VGA
    # send 'W' to UART TX
    ecall
```

---

## What This Demo Proves

- CPU executes a complete interactive program (not just crypto)
- UART input works as keyboard simulation
- VGA output works as screen simulation
- Game loop (read -> process -> display -> repeat) works correctly
- The HADES simulator is a fully functional embedded system


---

## Alternative: Run from Real Assembly (requires cross-compiler)

If you have `riscv64-unknown-elf-gcc` installed, you can assemble and run the actual `.S` file:

```bash
make demo-08b
```

This does:
1. Assembles `programs/guess_game.S` -> `.elf` using `riscv64-unknown-elf-gcc`
2. Converts `.elf` -> `.bin` (raw binary) using `riscv64-unknown-elf-objcopy`
3. Loads the binary into HADES and plays the game

Both `demo-08` and `demo-08b` produce identical results (same game logic, same 288 bytes, same 103 cycles).

### Install cross-compiler (if not already installed)

```bash
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

### Expected output

```
--- Step 2: Assemble programs/guess_game.S ---
  Assembled -> guess_game.elf
  Converted -> guess_game.bin (288 bytes)

--- Step 3: Run game on HADES (288 bytes loaded at 0x1000) ---
  VGA Display:
  ┌────────────────────┐
  │ GUESS 1-9          │
  │ >3 LOW             │
  │ >7 HIGH            │
  │ >5 WIN!            │
  └────────────────────┘

  Result:  WON!
```


---

## Interactive Mode: Play the Game Yourself

```bash
make demo-08c
```

This launches an interactive session where YOU type guesses from the keyboard:

```
============================================================
DEMO 08c: Interactive 'Guess the Number'
============================================================

  The CPU has picked a secret number between 1 and 9.
  Type your guess (a single digit) and press Enter.
  Type 'q' to quit.

  ┌────────────────────┐
  │ GUESS 1-9          │
  └────────────────────┘

  Your guess (1-9): 3
  CPU says: >3 LOW
  Your guess (1-9): 7
  CPU says: >7 HIGH
  Your guess (1-9): 5
  CPU says: >5 WIN!

  You won in 3 guesses!
```

Each time you type a digit and press Enter:
1. Python sends it to the CPU via `cpu.uart_send([ord(digit)])`
2. CPU reads from UART, compares with secret, writes result to VGA
3. Python reads VGA character buffer and prints the CPU's response

The secret number is 5. Try to find it in as few guesses as possible!
