# Demo 01 — Minimal RV32I CPU

This demo shows how to set up the HADES environment, build the CPU engine, run regression tests, and observe power leakage traces.

---

## Prerequisites

- WSL2 with Ubuntu 22.04+
- Python venv activated: `source .venv/bin/activate`
- Python venv deactivated: `deactivate`
- Engine built: `make engine`
---

## 1. Install System Dependencies

Open WSL terminal:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    g++ \
    python3-full \
    python3-venv \
    python3-dev \
    gcc-riscv64-unknown-elf \
    binutils-riscv64-unknown-elf
```

---

## 2. Create Python Virtual Environment

```bash
python3 -m venv .venv      # only once in the creation
source .venv/bin/activate  # every time source it before using python evironment
# deactivate, to leave venv environment
```

Install Python packages:

```bash
pip install pybind11 numpy plotext
```

---

## 3. Verify Installation

```bash
g++ --version
riscv64-unknown-elf-gcc --version
python3 -m pybind11 --includes
python3 -c "import numpy; print('numpy OK')"
```

All commands should succeed without errors.

---

## 4. Navigate to Project

```bash
# Activate virtual environment
source .venv/bin/activate

# Go to the HADES project root
cd <project-path>/HADES
```

---

## 5. Build the Engine

```bash
make engine
```

Expected output:
```
mkdir -p build
g++ -O2 -Wall -shared -std=c++17 -fPIC ... -o build/hades.cpython-312-x86_64-linux-gnu.so
```

The compiled shared library is placed in the `build/` directory. The Makefile sets `PYTHONPATH=build` automatically when running tests or the runner.

---

## 6. Run Regression Tests

```bash
make test
```

This internally runs:
```bash
PYTHONPATH=build python3 layer4_software/test_cpu.py
```

Expected output:
```
==================================================
HADES Phase 1 - RV32I CPU Regression Tests
==================================================
  PASS: test_addi
  PASS: test_add_sub
  PASS: test_logic
  PASS: test_shift
  PASS: test_load_store
  PASS: test_branch
  PASS: test_jal
  PASS: test_lui
  PASS: test_loop_sum
  PASS: test_x0_hardwired
==================================================
Results: 10 passed, 0 failed, 10 total
All tests passed!
```

---

## 7. Assemble a RISC-V Program (Optional)

If you have the RISC-V cross-compiler installed:

```bash
make asm
```

This assembles `programs/test_basic.S` into `programs/test_basic.bin`.

---

## 8a. Quick Build + Run a Single Assembly File

Write your own program:
```bash
vi programs/my_test.S
```

Build and run in one step:
```bash
make run-asm FILE=src/programs/my_test.S
```

This assembles, converts to binary, and runs it with register dump.

---

## 8b. Run a Binary with the Runner

Using the Makefile helper:

```bash
make run ARGS="src/programs/test_basic.bin --dump-regs --dump-mem 0x20:8"
```

Or manually with PYTHONPATH:

```bash
PYTHONPATH=build python3 layer4_software/runner.py programs/test_basic.bin --dump-regs --dump-mem 0x20:8
```

Expected output (approximate):
```
Cycles: ~25
PC:     0x00001044

Registers:
  x5  (t0  ) = 0x00000037 (55)
  x28 (t3  ) = 0x00000020 (32)
  x30 (t5  ) = 0x00000055 (85)
  x31 (t6  ) = 0x000000ff (255)
  ...

Memory [0x0020:0x0028]:
  0x0020: 37 00 00 00 ff 00 00 00
```

---

## 9. Interactive Python Session

```bash
PYTHONPATH=build python3
```

```python
import hades
from asmpack import *

cpu = hades.CPU()

# Use the encoder functions from test_cpu.py
exec(open('src/software/test_cpu.py').read())

prog = to_bytes([
    encode_i(0xAB, ZERO, 0b000, T0, OP_IMM),  # t0 = 0xAB
    encode_i(0x55, ZERO, 0b000, T1, OP_IMM),  # t1 = 0x55
    encode_r(0, T1, T0, 0b100, T2, OP_REG),   # t2 = t0 ^ t1 = 0xFE
    ECALL,
])

cpu.load_program(list(prog))
cpu.run()

print(f"t0 = {cpu.get_reg(5):#x}")   # 0xab
print(f"t1 = {cpu.get_reg(6):#x}")   # 0x55
print(f"t2 = {cpu.get_reg(7):#x}")   # 0xfe
```

---

## 10. Clean Build Artifacts

```bash
make clean
```

This removes:
- `build/` directory (containing the `.so` file)
- `tools/programs/*.elf` and `tools/programs/*.bin`

---

## 11. Troubleshooting

| Problem | Solution |
|---------|----------|
| `ModuleNotFoundError: No module named 'hades'` | Run `make engine` first, use `PYTHONPATH=build` or run via `make test` |
| `g++: command not found` | `sudo apt install build-essential` |
| `python3-config: not found` | `sudo apt install python3-dev` |
| `pybind11 not found` | `pip install pybind11` in your venv |
| `riscv64-unknown-elf-gcc: not found` | `sudo apt install gcc-riscv64-unknown-elf` |
| Shared library ABI mismatch | Rebuild: `make clean && make engine` |
| `.so` in project root (old build) | `rm *.so` then `make clean && make engine` |

---

## What This Demo Proves

- ✅ RV32I CPU correctly executes arithmetic, logic, memory, branch, jump instructions
- ✅ Leakage engine records power trace proportional to Hamming Weight of register writes
- ✅ Hamming Distance model tracks bit transitions between consecutive writes
- ✅ Build artifacts are isolated in `build/` directory
- ✅ System is ready for Phase 2 (AES implementation + CPA attack)


---

## Quick Run (One Command)

```bash
make demo-01
```

This runs `demos/demo_01_basic_cpu.py` which executes all the examples above automatically.
