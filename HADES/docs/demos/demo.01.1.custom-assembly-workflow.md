# Demo 01.1 — Write, Build, and Run Custom RISC-V Assembly

This demo shows how to write your own RISC-V assembly programs and run them on the HADES simulator.

---

## Prerequisites

- Phase 1 completed (`make engine` succeeds)
- RISC-V cross-compiler installed (`riscv64-unknown-elf-gcc`)

---

## 1. Project Structure for Assembly

All assembly programs live in the top-level `programs/` directory:

```
HADES/
├── src/
├────── programs/
│     ├── README.md         # Template and rules
│     ├── link.ld           # Linker script (shared by all programs)
│     ├── test_basic.S      # Example: sum loop
│     └── my_program.S      # ← YOUR NEW PROGRAMS GO HERE
```

---

## 2. Write a New Program

Create `src/programs/fibonacci.S`:

```asm
    .text
    .global _start

# Compute first 10 Fibonacci numbers, store to memory at 0x0000
_start:
    li   t0, 0x00         # base address
    li   t1, 0            # fib(0) = 0
    li   t2, 1            # fib(1) = 1
    li   t3, 10           # count

    sb   t1, 0(t0)        # store fib(0)
    sb   t2, 1(t0)        # store fib(1)
    addi t0, t0, 2        # advance pointer
    addi t3, t3, -2       # count -= 2

loop:
    add  t4, t1, t2       # t4 = fib(n-2) + fib(n-1)
    sb   t4, 0(t0)        # store fib(n)
    mv   t1, t2           # shift
    mv   t2, t4
    addi t0, t0, 1
    addi t3, t3, -1
    bnez t3, loop

    ecall                  # halt
```

---

## 3. Build and Run (One Command)

```bash
make run-asm FILE=programs/fibonacci.S
```

This does three things:
1. Assembles `fibonacci.S` → `fibonacci.elf`
2. Converts `fibonacci.elf` → `fibonacci.bin` (raw binary)
3. Runs `fibonacci.bin` on the simulator with register + trace dump

Expected output:
```
Cycles: ~30
PC:     0x00001038

Registers:
  x5  (t0  ) = 0x0000000a (10)
  x6  (t1  ) = 0x00000015 (21)
  x7  (t2  ) = 0x00000022 (34)
  ...

Power trace (N samples):
  [   0] 0.0000
  [   1] 1.0000
  ...
```

---

## 4. Build All Programs at Once

```bash
make asm
```

This builds every `.S` file in `programs/` into a `.bin` file.

---

## 5. Run with Custom Options

After building, run with specific flags:

```bash
# Dump memory at 0x0000 (10 bytes = Fibonacci sequence)
make run ARGS="src/programs/fibonacci.bin --dump-regs --dump-mem 0x0:10"

---

## 6. Program Template

Use this as a starting point for any new program:

```asm
    .text
    .global _start

_start:
    # ─── Setup ───
    # Registers available: t0-t6, a0-a7, s0-s11
    # Memory: 0x0000-0x0FFF (data), stack at 0x4000

    # ─── Your code here ───


    # ─── End ───
    ecall                  # halt the CPU
```

---

## 7. Rules and Constraints

| Rule | Detail |
|------|--------|
| Entry point | Must be `_start` |
| Code address | Loaded at 0x1000 (set by linker script) |
| Data memory | 0x0000–0x0FFF (4KB, read/write) |
| Stack | Starts at 0x4000 (grows downward) |
| Halt | Use `ecall` instruction |
| Instructions | All RV32I supported (see `docs/riscv-instruction-sheet.md`) |

---

## 8. More Examples

### Example: Bubble Sort

```asm
    .text
    .global _start

# Sort 8 bytes at address 0x0000 using bubble sort
_start:
    li   a0, 0x00         # array base
    li   a1, 8            # array length

outer:
    addi a1, a1, -1       # n--
    beqz a1, done
    mv   t0, a0           # ptr = base
    mv   t1, a1           # inner count

inner:
    lbu  t2, 0(t0)        # t2 = arr[i]
    lbu  t3, 1(t0)        # t3 = arr[i+1]
    ble  t2, t3, no_swap
    sb   t3, 0(t0)        # swap
    sb   t2, 1(t0)
no_swap:
    addi t0, t0, 1
    addi t1, t1, -1
    bnez t1, inner
    j    outer

done:
    ecall
```

To run: pre-load data at 0x0000 using `--data` flag or modify the program to initialize data inline.

---

## 9. Troubleshooting

| Problem | Solution |
|---------|----------|
| `riscv64-unknown-elf-gcc: not found` | `sudo apt install gcc-riscv64-unknown-elf` |
| `undefined reference to _start` | Add `.global _start` and `_start:` label |
| Program runs forever | Ensure `ecall` is reachable (check branch logic) |
| Wrong results | Use `--dump-regs` and `--dump-mem` to inspect state |
| `make run-asm` fails | Check `FILE=` path is correct (relative to project root) |

---

## What This Demo Proves

- ✅ Any `.S` file can be built and run with a single command
- ✅ No need to modify Makefile or Python code to add new programs
- ✅ Full RV32I instruction set available for practice
- ✅ Power trace observable for every program (leakage analysis ready)