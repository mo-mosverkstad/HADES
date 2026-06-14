# RISC-V Assembly Programs

This directory contains RISC-V assembly programs that run on the HADES simulator.

## How to Add a New Program

1. Create a `.S` file in this directory:
   ```bash
   vi programs/my_program.S
   ```

2. Use this template:
   ```asm
       .text
       .global _start

   _start:
       # Your code here
       # ...

       ecall    # halt the CPU
   ```

3. Build it:
   ```bash
   make asm
   ```

4. Run it:
   ```bash
   make run ARGS="programs/my_program.bin --dump-regs"
   ```

   Or build + run a single file:
   ```bash
   make run-asm FILE=programs/my_program.S
   ```

## Rules

- Entry point must be `_start` (defined by linker script)
- Program is loaded at address `0x1000`
- Data memory is at `0x0000–0x0FFF` (4KB)
- Stack starts at `0x4000` (grows downward)
- Use `ecall` to halt the CPU
- All standard RV32I instructions are supported

## Memory Map

```
0x0000–0x0FFF  Data (read/write)
0x1000–0x3FFF  Code (read/execute)
0x4000–0x4FFF  Stack
```

## Register Convention

| Register | ABI Name | Usage |
|----------|----------|-------|
| x0 | zero | Always 0 |
| x1 | ra | Return address |
| x2 | sp | Stack pointer |
| x5–x7 | t0–t2 | Temporaries |
| x8–x9 | s0–s1 | Saved |
| x10–x17 | a0–a7 | Arguments |
| x28–x31 | t3–t6 | Temporaries |

## Examples

See existing files:
- `test_basic.S` — loop sum, XOR, memory store