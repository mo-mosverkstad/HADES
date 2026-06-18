# HADES — Codebase Analysis

---

## Phase 0. Introduction: What Is HADES? (Big Picture)

HADES is a software simulator for a hardware board with RISC-V processor. The simulator can be executed on Linux or WSL (using Windows), but behaves as a RISC-V processor executing machine code.

### 0.1 Real World vs HADES

In real world, according to the diagram below, after compiling through the code chain from the human-friendly high-level C code, machine code is loaded into the memory of hardware processor which then executes the program. During program execution, the power consumption and power trace can be detected by measuring power pin using an oscilloscope, enabling attackers to recover any processing data. This execution process, apart from the compiling toolchain, can be simulated using HADES.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        REAL WORLD (Physical Chip)                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Programmer writes C code                                               │
│       ↓ (gcc compiler)                                                  │
│  Assembly (.S file)                                                     │
│       ↓ (assembler: riscv64-as)                                         │
│  Machine code (binary, 0s and 1s)                                       │
│       ↓ (loaded into chip's flash/RAM)                                  │
│  Hardware CPU executes instructions                                     │
│       ↓ (transistors switch, current flows)                             │
│  Power consumption varies with data                                     │
│       ↓ (oscilloscope measures power pin)                               │
│  Power trace (analog signal → digitized)                                │
│       ↓ (statistical analysis)                                          │
│  Attacker recovers secret key                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                        HADES (Software Simulator)                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Programmer writes assembly OR                                          │
│  Python generates machine code directly (aes_program.py)                │
│       ↓                                                                 │
│  Machine code (list of 32-bit integers)                                 │
│       ↓ (loaded into simulated memory via load_program())               │
│  C++ CPU class interprets each instruction                              │
│       ↓ (on each register write, calls leak_.record())                  │
│  Leakage engine computes HW(value) + noise                              │
│       ↓ (appends to trace vector)                                       │
│  Python reads trace via get_power_trace()                               │
│       ↓ (numpy correlation analysis)                                    │
│  Attacker recovers secret key                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 0.2 Code Structure of Simulator

```
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 5: Experiment / Attack Scripts (Python)                           │
│ ─────────────────────────────────────────────────────────────────────── │
│ layer5_attacker/cpa.py, layer5_attacker/exp_cpa_basic.py                │
│                                                                         │
│ Role: The "attacker" — collects traces, runs statistical analysis,      │
│       recovers secret keys. This is what a real attacker does with      │
│       an oscilloscope and a computer.                                   │
│                                                                         │
│ Real-world equivalent: Attacker's lab bench                             │
│   • cpa.py              = oscilloscope + analysis laptop                │
│   • get_power_trace()   = current probe on VCC pin                      │
│   • numpy correlation   = MATLAB/Python statistical analysis            │
│   • random plaintexts   = signal generator feeding the board            │
│                                                                         │
│ The attacker NEVER runs code on the board. They:                        │
│   1. Provide inputs (chosen plaintexts)                                 │
│   2. Let the board compute (encryption)                                 │
│   3. Measure physical side-effects (power trace)                        │
│   4. Analyze offline (correlation → recover key)                        │
├─────────────────────────────────────────────────────────────────────────┤
│ LAYER 4: Program Generation (Python + ASM)                              │
│ ─────────────────────────────────────────────────────────────────────── │
│ layer4_software/aes_program.py, layer4_software/aes_ref.py              │
│ programs/*.S (RISC-V assembly: games, echo terminal, tests)             │
│                                                                         │
│ Role: Acts as "compiler + assembler" — generates machine code           │
│       (32-bit RISC-V instructions) from high-level description.         │
│       Also prepares data (S-box table, keys) to load into memory.       │
│                                                                         │
│ Real-world equivalent: Cross-compiler + flash programmer                │
│   • aes_program.py (generates code)  = gcc cross-compiler               │
│   • cpu.load_program(binary)         = JTAG flash write                 │
│   • runner.py calls cpu.run()        = power switch + clock crystal     │
│   • test_cpu.py reads registers      = JTAG debugger reading results    │
│                                                                         │
│ The scripts run on your PC (host tools), but the machine code they      │
│ produce crosses the boundary and executes ON the simulated board.       │
│ Like gcc: the compiler runs on your laptop, but its output (.bin)       │
│ runs on the target hardware.                                            │
├─────────────────────────────────────────────────────────────────────────┤
│ LAYER 3: Python ↔ C++ Bridge (pybind11)                                 │
│ ─────────────────────────────────────────────────────────────────────── │
│ layer3_bridge/bindings.cpp → build/hades.*.so                           │
│                                                                         │
│ Role: Translates Python calls into C++ method calls.                    │
│       Python list → std::vector, Python int → uint32_t, etc.            │
│       This is just plumbing — no simulation logic here.                 │
│                                                                         │
│ Direction: Python calls C++ (ONE-WAY ONLY).                             │
│       C++ never calls Python. The engine is a passive library.          │
│                                                                         │
│ Real-world equivalent: the CONNECTOR/SOCKET on the board                │
│       (BNC connector, JTAG header, DB9 serial port).                    │
│       The connector does no work — it just provides a standardized      │
│       interface so external tools can access the board's internals.     │
│                                                                         │
│ Call flow:                                                              │
│   Layer 5/4/2 (Python) ──calls──→ Layer 3 (pybind11) ──calls──→ Layer 1 │
│   (attacker/tools/peripherals)    (type translation)    (C++ engine)    │
│                                                                         │
│ Everything flows downward. C++ doesn't know Python exists.              │
├─────────────────────────────────────────────────────────────────────────┤
│ LAYER 2: Host-Side Peripheral Drivers (Python)                          │
│ ─────────────────────────────────────────────────────────────────────── │
│ layer2_peripherals/__init__.py                                          │
│                                                                         │
│ Role: The "other end" of simulated I/O devices.                         │
│       Bridges between real world (keyboard, terminal) and simulated     │
│       hardware (UART FIFO, VGA buffer).                                 │
│       - TerminalDisplay: reads VGA char+color → renders ANSI colors     │
│       - TerminalInput: reads keyboard → sends to UART                   │
│       - AssemblyLoader: gcc toolchain → binary → load into CPU          │
│                                                                         │
│ What does Layer 2 represent in the real world?                          │
│                                                                         │
│ Both of the following real-world setups are valid interpretations:      │
│                                                                         │
│ Mapping 1: Direct connection (embedded kiosk / ATM / arcade)            │
│                                                                         │
│   Keyboard ──USB──→ Board UART RX     (no PC involved)                  │
│   VGA Monitor ←──VGA cable── Board VGA output                           │
│                                                                         │
│   Layer 2 = the keyboard and monitor directly wired to the board.       │
│                                                                         │
│ Mapping 2: PC in the middle (developer workstation)                     │
│                                                                         │
│   Keyboard → PC → PuTTY ──serial cable──→ Board UART RX                 │
│   Monitor ← PC ← PuTTY ←──serial cable── Board UART TX                  │
│                                                                         │
│   Layer 2 = the PC + terminal emulator that forwards bytes.             │
│                                                                         │
│ The UART device on the board doesn't know or care which setup is        │
│ used — it just sees bytes arriving in its RX FIFO and bytes leaving     │
│ its TX FIFO. Layer 2 sits at the cable boundary regardless.             │
│                                                                         │
│ Which mapping applies depends on the demo:                              │
│   demo_12c/13 (interactive) → Mapping 1 (direct keyboard/display)       │
│   CERBERUS run_shell.py     → Mapping 2 (PC terminal emulator)          │
│   CERBERUS run.py           → Automated test equipment (neither)        │
├─────────────────────────────────────────────────────────────────────────┤
│ LAYER 1: Hardware Simulator (C++)                                       │
│ ─────────────────────────────────────────────────────────────────────── │
│ layer1_hardware/src/cpu.cpp + layer1_hardware/include/*.h               │
│                                                                         │
│ Role: THIS IS THE CORE. Simulates what real silicon does:               │
│                                                                         │
│ How C++ simulates hardware:                                             │
│   Hardware = state (flip-flops) + logic (gates) + clock (tick)          │
│   C++     = variables (regs_[]) + code (switch/case) + loop (run(N))    │
│                                                                         │
│ Each cpu.run(1) call = one clock cycle:                                 │
│   1. Tick I/O devices (timer counts down, UART shifts bits)             │
│   2. Check interrupts (timer fired? jump to handler)                    │
│   3. Fetch instruction at PC (read memory[PC])                          │
│   4. Decode (extract opcode, registers, immediate)                      │
│   5. Execute (ALU: add/sub/shift/compare/branch)                        │
│   6. Memory access (load/store to RAM or I/O device)                    │
│   7. Write result to register (regs_[rd] = result)                      │
│   8. Record leakage (power_trace.push_back(HW(result)))                 │
│   9. Advance PC (pc_ += 4 or branch target)                             │
│                                                                         │
│ This is EXACTLY what real hardware does every clock cycle.              │
│ Only difference: real silicon does it in 1ns with transistors,          │
│ C++ does it in 1µs with software. Behavior is identical.                │
│                                                                         │
│   Real silicon component          →  C++ equivalent                     │
│   ───────────────────────────────────────────────────────────────────── │
│   32 flip-flop registers           →  uint32_t regs_[32]                │
│   Program counter register         →  uint32_t pc_                      │
│   ALU (adder, shifter, mux)        →  execute_alu() function            │
│   Instruction decoder              →  decode(instr) function            │
│   Pipeline registers               →  StageIFID, StageEX, StageMEMWB    │
│   SRAM cells (1MB)                 →  vector<uint8_t> data_             │
│   L1 cache (tag + data arrays)     →  Cache class (lines[], tags[])     │
│   SDRAM row buffer                 →  SDRAMModel (current_row_)         │
│   Timer countdown register         →  Timer class (counter_)            │
│   UART TX/RX FIFOs                 →  queue<uint8_t> rx_fifo_           │
│   I/O address decoder              →  IOBus (if addr>=0xF000, route)    │
│   Power rail current               →  Leakage class (HW/HD model)       │
│   Clock crystal oscillation        →  for loop in cpu.run(N)            │
│                                                                         │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │ CPU Core (3-stage pipeline + forwarding)                      │     │
│   │  IF/ID → EX → MEM/WB, CSRs, performance counters              │     │
│   └───────────────────────────────────────────────────────────────┘     │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │ Memory Hierarchy                                              │     │
│   │  L1 I-Cache + D-Cache (2KB, DM, 32B) → On-chip RAM → SDRAM    │     │
│   └───────────────────────────────────────────────────────────────┘     │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │ I/O Devices (memory-mapped)                                   │     │
│   │  Timer (0xF000) │ UART (0xF020) │ GPIO (0xF040)               │     │
│   │  Mutex (0xF060) │ VGA  (0xF080, char+color+pixel)             │     │
│   └───────────────────────────────────────────────────────────────┘     │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │ Leakage Engine + Countermeasures + Fault Injection            │     │
│   │  HW/HD model, noise, masking, constant-power, shuffled        │     │
│   │  Cycle-precise register bit-flip faults                       │     │
│   └───────────────────────────────────────────────────────────────┘     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 0.3 What Each Layer Simulates (Mapping to Real World)

| Layer | In HADES | In Real World | What it replaces |
|-------|----------|---------------|------------------|
| 5 | `layer5_attacker/cpa.py` | Attacker with oscilloscope + laptop | Lab equipment + analysis software |
| 4 | `layer4_software/aes_program.py` | Compiler + assembler + linker | `gcc` + `as` + `ld` + `objcopy` |
| 3 | `layer3_bridge/bindings.cpp` | (no equivalent) | Just glue between Python and C++ |
| 2 | `layer2_peripherals/` | Host PC connected to device (terminal, debugger) | Serial terminal + JTAG probe |
| 1+2 | `layer1_hardware/src/cpu.cpp` + `layer1_hardware/include/*` | Silicon CPU + SRAM + peripherals | The actual processor die |
| 1 | `layer1_hardware/include/leakage.h` | Transistor physics (CMOS switching) | Oscilloscope probe on VDD pin |

### 0.4 Execution Flow (Phase 2: CPA Attack)

Step-by-step what happens when you run `make cpa`:

```
┌─────────────────────────────────────────────────────────────────┐
│ Step 1: Python (layer5_attacker/cpa.py) starts                  │
│                                                                 │
│   for i in range(500):  # collect 500 traces                   │
│       plaintext = random_16_bytes()                             │
│       ↓                                                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 2: Generate machine code (aes_program.py)                  │
│                                                                 │
│   code = generate_aes128_program()  # ~600 RV32I instructions   │
│   data = generate_data(plaintext, key)  # sbox + expanded keys  │
│                                                                 │
│   code is a list of uint32 values like:                         │
│   [0x00050513,  # addi a0, zero, 0  (a0 = state base)          │
│    0x10000593,  # addi a1, zero, 256 (wrong, just example)      │
│    ...]                                                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 3: Load into simulator (pybind11 → C++)                    │
│                                                                 │
│   cpu = hades.CPU()                                             │
│   cpu.load_data(data, 0x0000)     # sbox, keys → mem[0x0000]   │
│   cpu.load_program(binary, 0x1000) # code → mem[0x1000]        │
│                                                                 │
│   Memory now looks like:                                        │
│   [0x0000] plaintext bytes (16)                                 │
│   [0x0010] key bytes (16)                                       │
│   [0x0100] S-box table (256)                                    │
│   [0x0200] expanded round keys (176)                            │
│   [0x1000] first instruction of AES program                     │
│   [0x1004] second instruction                                   │
│   ...                                                           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 4: Execute (C++ cpu.cpp — THE HARDWARE SIMULATION)         │
│                                                                 │
│   pc = 0x1000                                                   │
│   while not halted:                                             │
│       instr = mem.read_word(pc)    # FETCH                      │
│       opcode = instr & 0x7F        # DECODE                     │
│       ... extract rd, rs1, rs2 ... #                            │
│       result = alu_compute(...)    # EXECUTE                    │
│       write_reg(rd, result)        # WRITEBACK + LEAK           │
│           └→ leak_.record(result)  # power = HW(result) + noise │
│              trace_.push_back(power)                             │
│       pc += 4                      # NEXT                       │
│                                                                 │
│   Example for SubBytes LBU instruction:                         │
│       instr = 0x0002C283  (lbu t0, 0(t1))                       │
│       addr = regs[t1] + 0 = 0x0100 + state_byte_value           │
│       value = mem.read_byte(addr)  = SBOX[state_byte]           │
│       write_reg(t0, value)                                      │
│           → leak_.record(value)                                 │
│           → trace_.push_back(popcount(value) + noise)           │
│                                                                 │
│   After ~5000 instructions: halted = true (ECALL)               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 5: Extract trace (Python reads back from C++)              │
│                                                                 │
│   trace = cpu.get_power_trace()  # ~3000 float values           │
│   # Each value = HW of a register write + noise                 │
│   # trace[50] might be HW(SBOX[plaintext[0] ^ key[0]])         │
│                                                                 │
│   traces.append(trace)  # store for later analysis              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 6: Statistical Attack (after collecting all 500 traces)    │
│                                                                 │
│   for key_byte in range(16):                                    │
│       for guess in range(256):                                  │
│           predicted = [HW(SBOX[pt[i][byte] ^ guess])            │
│                        for i in range(500)]                     │
│           measured = [traces[i][sbox_index[byte]]               │
│                       for i in range(500)]                      │
│           correlation = numpy.corrcoef(predicted, measured)      │
│                                                                 │
│       best_guess = argmax(|correlation|)                         │
│       # For correct guess: correlation ≈ 0.9                    │
│       # For wrong guesses: correlation ≈ 0.0                    │
│                                                                 │
│   Result: recovered_key = [0x2b, 0x7e, 0x15, ...] ✅            │
└─────────────────────────────────────────────────────────────────┘
```

### 0.5 What Is NOT Simulated (Current Limitations)

| Feature | Status | Impact |
|---------|--------|--------|
| Pipeline (IF/ID/EX/MEM/WB) | Phase 3 | No timing variation from hazards |
| Cache (hit/miss latency) | Phase 4 | No cache timing attacks possible |
| I/O devices (UART, timer, GPIO) | Phase 7 | No interrupt-driven programs, no I/O timing attacks |
| Interrupts + CSRs | Phase 7 | No context-switch leakage |
| Virtual memory / MMU | Not planned | Flat address space is sufficient |
| Multi-core | Future | No cross-core attacks yet |
| Bus arbitration | Not planned | Single-master system |

The current simulator is a **single-cycle, single-core, no-cache CPU** — the simplest possible architecture that still produces meaningful side-channel leakage.

## Phase 1. Core Concepts of RISC-V and Minimal RISC-V Processor

RISC-V is an open-standard instruction set architecture. RV32I is the 32-bit integer base:
- Fixed 32-bit instruction width
- 32 general-purpose registers (x0 always 0)
- Load/store architecture (only loads/stores access memory)
- 6 instruction formats: R, I, S, B, U, J

**Instruction decoding** extracts fields by bit position:
```
bits [6:0]   → opcode (determines instruction type)
bits [11:7]  → rd (destination register)
bits [14:12] → funct3 (sub-operation selector)
bits [19:15] → rs1 (source register 1)
bits [24:20] → rs2 (source register 2)
bits [31:25] → funct7 (further differentiation, e.g., ADD vs SUB)
```

### 1.1 Memory (`src/hardware/memory.h`)

Header-only implementation of a flat 64KB memory space.

**Design decisions**:
- `std::vector<uint8_t>` backing store (heap-allocated, safe)
- Address masking `addr & (SIZE - 1)` prevents out-of-bounds (wraps around)
- Little-endian byte ordering for multi-byte reads/writes
- No alignment enforcement (simplifies implementation; real RISC-V may trap on misaligned access)

**Key methods**:
- `read_byte/half/word` — load from memory with appropriate width
- `write_byte/half/word` — store to memory
- `load(base, bytes)` — bulk load (used for program/data initialization)
- `dump(addr, len)` — bulk read (used for observation)

### 1.2 CPU (`src/hardware/cpu.h`, `src/hardware/cpu.cpp`)

The core simulator implementing single-cycle RV32I execution.

**State**:
- `regs_[32]` — register file (x0 enforced to 0 after every instruction)
- `pc_` — program counter (initialized to 0x1000)
- `cycles_` — instruction count (1 cycle per instruction in single-cycle mode)
- `halted_` — stop flag (set by ECALL)

**Execution loop** (`run()`):
```
while not halted and count < max:
    instr = mem.read_word(pc)
    execute(instr)
    cycles++
```

**Instruction decode** (`execute()`):
1. Extract opcode (bits [6:0])
2. Switch on opcode to determine format
3. Extract remaining fields based on format
4. Compute result
5. Write result via `write_reg()` (triggers leakage)
6. Advance PC (or set PC for branches/jumps)

**Immediate decoding**:
Each format has a different immediate encoding with sign extension:
- `imm_i`: bits [31:20], sign-extended from bit 11
- `imm_s`: bits [31:25] | [11:7], sign-extended from bit 11
- `imm_b`: bits [31|7|30:25|11:8] << 1, sign-extended from bit 12
- `imm_u`: bits [31:12] << 12 (no sign extension needed, already 32-bit)
- `imm_j`: bits [31|19:12|20|30:21] << 1, sign-extended from bit 20

**Critical design choice — `write_reg()`**:
```cpp
void CPU::write_reg(uint32_t rd, uint32_t value) {
    if (rd == 0) return;  // x0 is always 0
    regs_[rd] = value;
    leak_.record(value);  // ← LEAKAGE POINT
}
```
Every register write emits a leakage sample. This is the hook that enables all side-channel attacks.

### 1.3 pybind11 Bindings (`src/bridge/bindings.cpp`)

Exposes the C++ engine as a Python module named `hades`.

**Exposed interface**:
- `hades.CPU()` — constructor
- `cpu.load_program(bytes, base_addr)` — load code
- `cpu.load_data(bytes, base_addr)` — load data
- `cpu.run(max_instructions)` — execute
- `cpu.reset()` — clear state
- `cpu.get_power_trace()` → `list[float]`
- `cpu.get_cycles()` → `int`
- `cpu.get_pc()` → `int`
- `cpu.get_reg(idx)` → `int`
- `cpu.read_mem(addr, len)` → `list[int]`
- `cpu.set_leakage_model(model)` — HW or HD
- `cpu.set_noise(stddev)` — noise level
- `cpu.set_seed(seed)` — RNG seed

**pybind11 features used**:
- `py::class_<CPU>` — wrap C++ class
- `py::enum_<LeakageModel>` — wrap enum
- `pybind11/stl.h` — automatic `std::vector` ↔ Python list conversion
- `py::arg()` — named arguments with defaults

### 1.4 Build System (`Makefile`)

Two build targets:
1. **engine**: Compiles C++ sources into `hades.cpython-3XX-x86_64-linux-gnu.so`
   - Uses `python3 -m pybind11 --includes` for header paths
   - Uses `python3-config --extension-suffix` for correct output filename
2. **asm**: Assembles `.S` files to `.bin` via RISC-V cross-compiler
   - `.S` → `.elf` (gcc with linker script)
   - `.elf` → `.bin` (objcopy, raw binary)

### 1.5 Linker Script (`src/programs/link.ld`)

Defines memory layout for assembled programs:
```
CODE:  0x1000 (12KB) — program instructions
DATA:  0x0000 (4KB)  — plaintext, keys, S-box, results
STACK: 0x4000 (4KB)  — stack space
```

This matches the CPU's initial PC (0x1000) and the memory map defined in study.md.

link.ld is a linker script that acts as the memory-map contract between the RISC-V cross-compiler and the HADES CPU simulator.

What it does:

When you run make asm, the cross-compiler produces a .bin file. The linker script controls where each section ends up in that binary:

| Section | Address | Purpose |
|---|---|---|
| .text (code) | 0x1000 | Instructions |
| .data / .bss | 0x0000 | Variables |
| Stack | 0x4000 | Stack space |

Why it exists:

The CPU simulator has hardcoded default addresses:

cpp
cpu.load_program(binary, base_addr=0x1000)  // expects code at 0x1000
cpu.load_data(data, base_addr=0x0000)       // expects data at 0x0000


The linker script ensures the compiled binary matches these expectations. Without it, the toolchain would place code at some default
address (like 0x10000), and the simulator would execute garbage at 0x1000.

### 1.6 Data Flow

```
Python script
    │
    ├─ load_program(binary)  → copies bytes into mem_[0x1000..]
    ├─ load_data(data)       → copies bytes into mem_[0x0000..]
    ├─ run()                 → executes instructions, fills leak_.trace_
    │
    ├─ get_power_trace()     → returns leak_.trace_ as Python list
    ├─ get_reg(i)            → returns regs_[i]
    └─ read_mem(addr, len)   → returns mem_.dump(addr, len)
```

## Phase 2: 3-Stage Pipeline

### 2.1 Pipeline Design (`layer1_hardware/include/pipeline.h`)

The DTEK-V uses a 3-stage pipeline (simpler than textbook 5-stage):

```
┌──────────┐    ┌──────────┐    ┌──────────┐
│  IF/ID   │───▶│    EX    │───▶│  MEM/WB  │
│          │    │          │    │          │
│ Fetch +  │    │ ALU      │    │ Memory   │
│ Decode   │    │ Branch   │    │ Writeback│
└──────────┘    └──────────┘    └──────────┘
```

Each stage is represented by a struct holding the instruction, PC, and computed values:
- `StageIFID`: fetched instruction + PC
- `StageEX`: ALU result, control flags (is_load, is_store, is_branch)
- `StageMEMWB`: final result to write back to register file

### 2.2 Forwarding

Data forwarding eliminates stalls for ALU→ALU dependencies:

```cpp
uint32_t CPU::forward_reg(uint32_t reg_idx) const {
    // Priority 1: forward from EX (most recent result)
    if (ex_.valid && ex_.writes_rd && ex_.rd == reg_idx && !ex_.is_load)
        return ex_.alu_result;
    // Priority 2: forward from MEM/WB
    if (memwb_.valid && memwb_.writes_rd && memwb_.rd == reg_idx)
        return memwb_.result;
    // No forwarding needed
    return regs_[reg_idx];
}
```

Key: loads CANNOT be forwarded from EX (data not available until MEM/WB), causing load-use stalls.

### 2.3 Hazard Detection

```cpp
bool CPU::detect_load_use_hazard() const {
    // EX has a load, and IF/ID needs that register
    if (!ex_.valid || !ex_.is_load || !ifid_.valid) return false;
    // Check if next instruction reads the load destination
    ...
}
```

When detected: insert 1-cycle bubble (stall IF/ID, let EX→MEM/WB proceed).

### 2.4 Branch Handling

Branches are resolved in EX stage. When taken:
1. Redirect PC to branch target
2. Flush IF/ID (the wrongly-fetched instruction)
3. Increment `stalls_branch` counter

This costs exactly 1 cycle per taken branch.

### 2.5 Performance Counters

```cpp
struct PerfCounters {
    uint64_t mcycle;        // total clock cycles
    uint64_t minstret;      // retired instructions
    uint64_t stalls_data;   // load-use hazard stalls
    uint64_t stalls_branch; // branch penalty cycles
};
```

Accessible via CSR instructions (CSRRW/CSRRS/CSRRC) or Python API.

### 2.6 C++ Architecture: Why Not Polymorphism?

#### The Problem

Two CPU implementations exist:
- `CPU` — single-cycle, simple, working (Phase 1)
- `PipelinedCPU` — 3-stage pipeline with hazards, forwarding, perf counters

They share a common API surface (`load_program`, `run`, `get_cycles`, `get_reg`, etc.) but differ in behavior and extra capabilities. The natural OOP instinct (especially from Java) is to create an interface:

```cpp
class CPUI {
    virtual void run(uint32_t max) = 0;
    virtual uint64_t get_cycles() const = 0;
    // ...
};
class CPU : public CPUI { ... };
class PipelinedCPU : public CPUI { ... };
```

#### Why This Doesn't Fit Here

**1. The two classes are not truly interchangeable.**

`PipelinedCPU` exposes pipeline-specific data (`get_instret()`, `get_perf()`, `stalls_data`, `stalls_branch`) that has no meaning on the single-cycle CPU. Code using the pipelined version will always need to know it's pipelined:

```python
# This defeats polymorphism — you need to know the concrete type
if isinstance(cpu, hades.PipelinedCPU):
    print(cpu.get_perf())
```

**2. No C++ code needs runtime dispatch.**

There is no `std::vector<CPUI*>` holding a mix of CPUs. There is no factory returning `CPUI*`. Python picks one at construction time and uses it directly. The dispatch happens in Python, not C++.

**3. Virtual inheritance adds cost and complexity without benefit.**

| Aspect | Virtual inheritance | Separate classes |
|--------|--------------------|-----------------------|
| Runtime cost | vtable lookup per virtual call | zero |
| Inlining | blocked (compiler can't see through vtable) | fully inlinable |
| Object size | larger (vtable pointer) | minimal |
| Cache locality in `run()` loop | worse (larger `this`) | better |
| Shared state in base class | leads to fragile design, slicing bugs | each class owns its own state cleanly |
| Diamond problem risk | yes, if hierarchy grows | impossible |
| Compile-time error on interface mismatch | yes | no (solved by concepts, see below) |

**4. Performance: not about 1-2ns per call.**

The vtable cost per call is trivial. The real issue is that `virtual` prevents inlining. In the hot loop (`run()` calling `execute()`, `forward_reg()`, `write_reg()`), inlining is critical — the compiler can eliminate redundant loads, fold constants, and vectorize. Virtual blocks all of this.

#### Where Polymorphism IS Appropriate in C++

Use `virtual` when:
- **Plugin systems**: load a DLL at runtime, get a `Base*` back, concrete type unknown at compile time
- **Heterogeneous collections**: `std::vector<std::unique_ptr<Shape>>` holding circles, rectangles, etc.
- **Factory patterns**: caller genuinely cannot know the concrete type
- **Framework callbacks**: registering event handlers that the framework calls later

None of these apply to HADES. The caller always knows whether it created a `CPU` or `PipelinedCPU`.

#### The C++ Solution: Concepts + static_assert

C++20 concepts enforce interface contracts at compile time with zero runtime cost:

```cpp
// cpu_concept.h
#pragma once
#include <concepts>
#include <cstdint>
#include <vector>

template<typename T>
concept CPULike = requires(T cpu, std::vector<uint8_t> bin, uint32_t addr, uint32_t len) {
    { cpu.load_program(bin, addr) } -> std::same_as<void>;
    { cpu.load_data(bin, addr) } -> std::same_as<void>;
    { cpu.run(1u) } -> std::same_as<void>;
    { cpu.reset() } -> std::same_as<void>;
    { cpu.get_cycles() } -> std::same_as<uint64_t>;
    { cpu.get_pc() } -> std::same_as<uint32_t>;
    { cpu.get_reg(0u) } -> std::same_as<uint32_t>;
    { cpu.read_mem(addr, len) } -> std::same_as<std::vector<uint8_t>>;
};

static_assert(CPULike<CPU>);
static_assert(CPULike<PipelinedCPU>);
```

If someone changes `CPU::get_cycles()` to return `int` instead of `uint64_t`, the build fails immediately.

#### Python-Side Polymorphism (Free)

Python's duck typing provides polymorphism without any C++ mechanism:

```python
def run_and_report(cpu):  # accepts CPU or PipelinedCPU
    cpu.run()
    print(cpu.get_cycles())

run_and_report(hades.CPU())
run_and_report(hades.PipelinedCPU())
```

For stricter type checking (mypy, pydantic), use `Protocol`:

```python
from typing import Protocol

class CPULike(Protocol):
    def run(self, max_instructions: int = ...) -> None: ...
    def get_cycles(self) -> int: ...
    def get_reg(self, idx: int) -> int: ...
```

Both `hades.CPU` and `hades.PipelinedCPU` satisfy this protocol automatically — no C++ changes needed.

#### Enforcing Binding Consistency (Macro)

To prevent the pybind11 bindings from diverging:

```cpp
#define BIND_CPU_COMMON(cls, name) \
    py::class_<cls>(m, name) \
        .def(py::init<>()) \
        .def("load_program", &cls::load_program, py::arg("binary"), py::arg("base_addr") = 0x1000) \
        .def("load_data", &cls::load_data, py::arg("data"), py::arg("base_addr") = 0x0000) \
        .def("run", &cls::run, py::arg("max_instructions") = 1000000) \
        .def("reset", &cls::reset) \
        .def("get_power_trace", &cls::get_power_trace) \
        .def("get_cycles", &cls::get_cycles) \
        .def("get_pc", &cls::get_pc) \
        .def("get_reg", &cls::get_reg, py::arg("idx")) \
        .def("read_mem", &cls::read_mem, py::arg("addr"), py::arg("len"))

// Usage in bindings.cpp:
BIND_CPU_COMMON(CPU, "CPU");
BIND_CPU_COMMON(PipelinedCPU, "PipelinedCPU")
    .def("get_instret", &PipelinedCPU::get_instret)
    .def("get_perf", ...);  // pipeline-specific extras
```

If one class is missing a common method, the macro expansion fails at compile time.

#### Summary

| Approach | When to use | Cost |
|----------|-------------|------|
| C++ virtual (runtime polymorphism) | Collections of mixed types, plugins, factories | vtable + no inlining |
| C++ concepts + static_assert | Compile-time interface contract between independent classes | zero runtime |
| Python Protocol | Type-checked duck typing on the Python side | zero (annotation only) |
| Binding macro | Ensure pybind11 exposes consistent API | zero runtime |

**HADES uses**: separate classes + concept check + binding macro. This gives interface safety without inheritance overhead.

#### File Layout (Final)

```
src/hardware/
├── decode.h            # Shared: Decoded struct, decode(), AluResult, execute_alu()
├── leakage.h           # Shared: Leakage class (HW/HD model + noise + trace)
├── memory.h            # Shared: Memory class (64KB flat, header-only)
├── pipeline.h          # Pipeline stage structs + PerfCounters
├── cpu.h / cpu.cpp     # Single-cycle CPU (Phase 1, unchanged interface)
├── pipelined_cpu.h/cpp # 3-stage pipelined CPU (Phase 2)
└── cpu_concept.h       # CPULike concept + static_assert (compile-time check)
```


### 2.7 Why Not a Single Class with a Mode Flag?

An earlier attempt merged both processors into one class with `set_pipeline_enabled(bool)`. This is worse than separate classes:

**Problems with the flag approach:**
- **Dead state**: pipeline registers sit unused in single-cycle mode; `cycles_` counter is redundant in pipeline mode
- **Branching pollution**: every method gets `if (pipeline_enabled_)` checks
- **Scales poorly**: adding a multi-cycle processor means `if (mode == SINGLE) ... else if (mode == PIPELINE) ... else if (mode == MULTICYCLE) ...` scattered throughout
- **Testing risk**: one mode's test can accidentally touch the other mode's state
- **Readability**: you can't read one execution model without mentally filtering out the other

**Separate classes have:**
- Zero performance overhead (no flags, no virtual, no branching)
- Each file is self-contained and independently readable
- Adding a new processor type = adding a new file, no existing code touched
- `static_assert(CPULike<NewCPU>)` ensures API consistency at compile time

**Design rule**: if two things have different internal behavior and different state, they should be different classes — even if their external API overlaps. Share code via composition (Memory, Leakage) and free functions (decode, execute_alu), not inheritance or flags.

## Phase 3: Cache

### 3.1 Cache Design (`layer1_hardware/include/cache.h`)

Matches DTEK-V specification:
- 2KB total, 64 lines, 32 bytes per block
- Direct-mapped (each address maps to exactly one line)
- Write-through (stores always go to memory)

```
Address: [tag (21 bits) | index (6 bits) | offset (5 bits)]
         addr >> 11       (addr>>5)&0x3F   addr & 0x1F
```

### 3.2 Hit/Miss Logic

```cpp
bool Cache::access(uint32_t addr) {
    uint32_t index = (addr >> OFFSET_BITS) & (NUM_LINES - 1);
    uint32_t tag = addr >> TAG_SHIFT;

    if (lines_[index].valid && lines_[index].tag == tag) {
        hits_++;
        return true;   // HIT: 1 cycle
    } else {
        lines_[index] = {true, tag};  // allocate
        misses_++;
        return false;  // MISS: pay miss_penalty + memory latency
    }
}
```

### 3.3 Integration with CPU

Two cache instances:
- **I-Cache**: checked on every instruction fetch (pipeline mode)
- **D-Cache**: checked on every load/store

On miss: `perf_.mcycle += miss_penalty + mem_hierarchy_latency`

### 3.4 Common Interface Refactor (`cpu_concept.h`)

Formalized the shared API between `CPU` and `PipelinedCPU` using a C++20 concept:

```cpp
template<typename T>
concept CPULike = requires(T cpu, ...) {
    cpu.load_program(bin, addr);
    cpu.load_data(bin, addr);
    cpu.run(1u);
    cpu.reset();
    cpu.get_cycles();
    cpu.get_pc();
    cpu.get_reg(0u);
    cpu.read_mem(addr, len);
};
```

Both classes are statically asserted to satisfy this contract.

### 3.5 CRTP Base Class (`cpu_base.h`)

Extracted shared state and accessor implementations into `CPUBase<Derived>`:

**State:**
- `regs_[32]`, `pc_`, `halted_`, `mem_`
- `icache_`, `dcache_`, `cache_enabled_`, `miss_penalty_`

**Methods provided by base:**
- `load_program()`, `load_data()`
- `get_pc()`, `get_reg()`, `read_mem()`
- `set_cache_enabled()`, `set_miss_penalty()`, `get_icache_misses()`, `get_dcache_misses()`
- `write_reg()` (protected)

Zero vtable overhead — each instantiation (`CPUBase<CPU>`, `CPUBase<PipelinedCPU>`) is a fully independent class resolved at compile time.

### 3.6 Shared ISA Decode (`rv32_decode.h`)

Extracted the parts genuinely common to both execution models:

- `RV32_Opcode` enum (was duplicated in both .cpp files)
- `Decoded` struct + `decode_instr()` inline function

**Not included here:** `ExecResult` and `execute_alu()` — these produce pipeline-stage metadata (`is_branch`, `branch_taken`, `branch_target`, `store_value`) that only the pipelined model needs. The single-cycle CPU executes directly via a switch on decoded fields.

### 3.7 Cache Integration

Caches moved to `CPUBase` so both processors can use them uniformly. Each CPU's execution loop is responsible for:
- Calling `icache_.access(pc_)` on instruction fetch
- Calling `dcache_.access(addr)` on loads
- Calling `dcache_.write_access(addr)` on stores
- Adding `miss_penalty_` to its own cycle counter on misses

### 3.8 What Stays Unique to Each Class

| `CPU` | `PipelinedCPU` |
|-------|----------------|
| `cycles_` counter | `PerfCounters perf_` (mcycle, minstret, stalls_data, stalls_branch) |
| Single `execute()` switch | Pipeline stages (IF/ID → EX → MEM/WB) |
| Direct register reads | Forwarding logic (`forward_reg()`) |
| — | Hazard detection (`detect_load_use_hazard()`) |
| — | `ExecResult` / `execute_alu()` (structured output for pipeline) |
| — | CSR registers |

---

### 3.9 Memory Subsystem Refactoring

#### Problem (Phase 1-3 design)

Cache checks, hierarchy latency computation, and raw memory access were scattered across both CPU implementations:

```cpp
// Duplicated in cpu.cpp AND pipelined_cpu.cpp:
if (cache_enabled_ && !dcache_.access(addr))
    cycles_ += miss_penalty_ + mem_.compute_latency(addr);
uint32_t val = mem_.read_word(addr);
```

Each CPU had to manually orchestrate cache → hierarchy → raw access. Adding a new memory feature required editing every CPU.

#### Solution: Composite Memory Object (Implemented)

A single `Memory` class (composite pattern) owns the entire memory subsystem. Sub-components are accessed via getters for chaining.

**File**: `src/hardware/memory.h`

```
Memory                              (single composite, owned by CPUBase)
  ├─ MemHierarchy hierarchy_        (64KB backing store + SDRAM latency model)
  ├─ MemoryPort icache_             (instruction cache port)
  └─ MemoryPort dcache_             (data cache port)
```

**Classes**:

- `MemoryPort` — one access path with its own L1 cache. Handles cache lookup, latency accumulation, and penalty draining. References the shared `MemHierarchy`.
- `Memory` — composite owning `MemHierarchy` + two `MemoryPort` instances. Provides global config methods and direct load/dump (bypassing cache).

**CPU usage (chaining)**:

```cpp
// CPUBase holds single member:
Memory mem_;

// Instruction fetch:
uint32_t instr = mem_.icache().read_word(pc_);
perf_.mcycle += mem_.icache().drain_penalty();

// Data load:
uint32_t val = mem_.dcache().read_word(addr);
perf_.mcycle += mem_.dcache().drain_penalty();

// Data store (drain but don't stall — write buffer model):
mem_.dcache().write_word(addr, val);
mem_.dcache().drain_penalty();

// Configuration (from Python bindings):
mem_.set_hierarchy_enabled(true);
mem_.set_cache_enabled(true);
mem_.set_miss_penalty(20);

// Stats:
mem_.sdram().get_row_hits();
mem_.icache().get_cache_misses();
```

**CPUBase is now minimal**:

```cpp
template<typename Derived>
class CPUBase {
protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;
    Memory mem_;                  // ← single member for all memory
    void write_reg(uint32_t rd, uint32_t value);
public:
    // All config/stats delegate to mem_:
    void set_cache_enabled(bool e)        { mem_.set_cache_enabled(e); }
    void set_miss_penalty(uint32_t c)     { mem_.set_miss_penalty(c); }
    void set_mem_hierarchy_enabled(bool e) { mem_.set_hierarchy_enabled(e); }
    uint64_t get_icache_misses() const    { return mem_.icache().get_cache_misses(); }
    uint64_t get_dcache_misses() const    { return mem_.dcache().get_cache_misses(); }
    uint64_t get_sdram_row_hits() const   { return mem_.sdram().get_row_hits(); }
    uint64_t get_sdram_row_misses() const { return mem_.sdram().get_row_misses(); }
};
```

#### Design Benefits

| Property | Result |
|----------|--------|
| Single ownership | One `mem_` object, one `reset()`, one lifetime |
| CPU code simplicity | No cache/hierarchy logic in CPU — just read/write + drain |
| Extensibility | Add L2, write buffer, bus contention inside `Memory` without touching CPUBase |
| Chaining clarity | `mem_.dcache().read_word(addr)` makes access path explicit |
| No constructor wiring | `Memory` internally constructs ports with hierarchy reference |
| Bindings unchanged | Python API still uses `cpu.set_cache_enabled()` etc. |

#### Files After Refactoring

| File | Content |
|------|---------|
| `src/hardware/memory.h` | `Memory` composite + `MemoryPort` class |
| `src/hardware/mem_hierarchy.h` | `MemHierarchy` (backing store) + `SDRAMModel` |
| `src/hardware/cache.h` | `Cache` (L1, direct-mapped, 2KB) |
| `src/hardware/cpu_base.h` | CRTP base with single `Memory mem_` |
| `src/hardware/cpu.cpp` | Simple CPU using `mem_.icache()` / `mem_.dcache()` |
| `src/hardware/pipelined_cpu.cpp` | Pipeline using same chaining pattern |

## Phase 4: Memory Hierarchy

### 4.1 Overview

Phase 4 replaces the flat `Memory` class with `MemHierarchy` — a unified memory controller that models realistic latency differences between on-chip RAM and off-chip SDRAM. The hierarchy is **disabled by default** for backward compatibility; when disabled, all accesses behave identically to the old flat 64KB memory.

### 4.2 File Structure

| File | Role |
|------|------|
| `src/hardware/mem_hierarchy.h` | `MemHierarchy` controller + `SDRAMModel` class |
| `src/hardware/cpu_base.h` | CRTP base holds `MemHierarchy mem_` (replaces old `Memory`) |
| `src/hardware/pipelined_cpu.cpp` | Integrates latency into pipeline stages (fetch + memory) |
| `src/bridge/bindings.cpp` | Exposes `set_mem_hierarchy_enabled`, `get_sdram_row_*` to Python |

### 4.3 Class Design

```
CPUBase<PipelinedCPU>
  │
  └─ MemHierarchy mem_          (replaces old Memory)
       │
       ├─ data_: vector<uint8_t>   (64KB flat storage, always used for actual data)
       ├─ enabled_: bool           (latency modeling on/off)
       ├─ onchip_latency_: uint32_t (default: 1 cycle)
       │
       └─ SDRAMModel sdram_
            ├─ current_row_: uint32_t   (tracks open row)
            ├─ row_bits_: uint32_t      (default: 10 → 1KB rows)
            ├─ row_hit_latency_: uint32_t  (default: 5 cycles)
            ├─ row_miss_latency_: uint32_t (default: 25 cycles)
            ├─ refresh_interval_: uint32_t (default: every 10000 accesses)
            └─ refresh_latency_: uint32_t  (default: 50 cycles)
```

Key design decision: `MemHierarchy` keeps the **same flat 64KB data store** regardless of hierarchy mode. The hierarchy only affects **latency computation**, not actual data routing. This means addresses always wrap with `& 0xFFFF` — the simulator does not model a larger address space.

### 4.4 Address Routing and Latency Model

```
MemHierarchy::compute_latency(addr)
    │
    ├─ hierarchy disabled? → return 0 (no extra cycles)
    │
    ├─ addr < 0x10000? → return onchip_latency_ (1 cycle)
    │
    └─ addr >= 0x10000? → SDRAMModel::access_latency(addr)
                            │
                            ├─ same row as last access? → row_hit_latency_ (5 cycles)
                            ├─ different row?           → row_miss_latency_ (25 cycles)
                            │
                            └─ + refresh_latency_ (50 cycles) if access_count % 10000 == 0
```

**Important**: Since all data wraps to 64KB, addresses >= 0x10000 still read/write from the same array. The SDRAM path is a latency model only — there is no distinct data for addresses >= 0x10000. In practice, the program counter lives at 0x1000 (on-chip range) and data loads can target any address.

### 4.5 Pipeline Integration

The latency is charged at two points in `pipelined_cpu.cpp`:

**1. Instruction Fetch (`stage_fetch_decode`)**

```cpp
if (cache_enabled_) {
    if (!icache_.access(pc_)) {
        perf_.mcycle += miss_penalty_ + mem_.compute_latency(pc_);
    }
} else if (mem_.is_enabled()) {
    perf_.mcycle += mem_.compute_latency(pc_);  // every fetch pays latency
}
```

When hierarchy is enabled without cache, **every** instruction fetch adds `onchip_latency_` (1 cycle) to the cycle count. This is realistic: even on-chip SRAM has non-zero access time.

**2. Data Load (`stage_memory`)**

```cpp
if (cache_enabled_) {
    if (!dcache_.access(addr)) {
        perf_.mcycle += miss_penalty_ + mem_.compute_latency(addr);
    }
} else if (mem_.is_enabled()) {
    perf_.mcycle += mem_.compute_latency(addr);  // every load pays latency
}
```

**3. Data Store (`stage_memory`)**

```cpp
if (mem_.is_enabled()) {
    mem_.compute_latency(addr, true);  // updates SDRAM row state, no stall added
}
```

Stores update the SDRAM row buffer state (so subsequent reads from the same row get hits) but do NOT add stall cycles — this models a write buffer.

### 4.6 Cycle Accounting Example

Given 4 load instructions + ECALL (5 instructions total), all addresses < 0x10000:

| Source | Count | Latency each | Total |
|--------|-------|-------------|-------|
| Base pipeline execution | — | — | 8 cycles |
| Instruction fetches (hierarchy) | 5 | 1 cycle | +5 cycles |
| Data loads (hierarchy) | 4 | 1 cycle | +4 cycles |
| ECALL fetch (already counted above) | — | — | +1 cycle |
| **Total** | | | **18 cycles** |

With SDRAM-range accesses to different rows: each load pays 25 cycles instead of 1, creating dramatic timing differences.

### 4.7 SDRAM Row Buffer Model Details

The `SDRAMModel` simulates row-buffer locality:

```cpp
uint32_t SDRAMModel::access_latency(uint32_t addr) {
    access_count_++;
    cycle_counter_++;

    // Periodic refresh stall
    uint32_t refresh_penalty = 0;
    if (cycle_counter_ % refresh_interval_ == 0) {
        refresh_penalty = refresh_latency_;  // 50 cycles
    }

    // Row buffer check
    uint32_t row = addr >> row_bits_;  // row = addr >> 10 → 1KB rows
    if (row == current_row_) {
        row_hits_++;
        return row_hit_latency_ + refresh_penalty;   // 5 + 0|50
    } else {
        current_row_ = row;
        row_misses_++;
        return row_miss_latency_ + refresh_penalty;  // 25 + 0|50
    }
}
```

Default row size = `1 << row_bits_` = 1024 bytes. Two addresses are in the same row if `addr >> 10` is equal. This means:
- 0x100, 0x101, 0x1FF → same row (row 0)
- 0x100, 0x500 → different rows (row 0 vs row 1)

### 4.8 Interaction with Cache (Phase 3 + Phase 4)

Three operational modes for any memory access:

| Cache | Hierarchy | Behavior |
|-------|-----------|----------|
| OFF | OFF | No extra latency (flat, Phase 1-2 behavior) |
| OFF | ON | Every access pays `compute_latency()` |
| ON | OFF | Miss pays `miss_penalty_` only |
| ON | ON | Miss pays `miss_penalty_ + compute_latency()` |

The combined mode (cache ON + hierarchy ON) is the most realistic:
- Cache hits: 0 extra cycles (data served from L1)
- Cache miss to on-chip: miss_penalty(20) + 1 = 21 cycles
- Cache miss to SDRAM row hit: miss_penalty(20) + 5 = 25 cycles
- Cache miss to SDRAM row miss: miss_penalty(20) + 25 = 45 cycles

### 4.9 Security Relevance

The SDRAM row buffer creates a **memory access pattern side-channel** independent of cache:

1. **Row-buffer timing attack**: Sequential access (same row) completes in 5 cycles; scattered access (different rows) takes 25 cycles. An attacker measuring total execution time can distinguish access patterns.

2. **Refresh-based jitter**: The periodic 50-cycle refresh stall occurs at fixed intervals, adding noise to timing measurements but also creating a deterministic marker.

3. **Combined with cache**: Even with L1 cache, cold misses during cryptographic operations hit the SDRAM model and leak the order/pattern of memory accesses through row hit/miss ratios.

This is exploitable for:
- Distinguishing AES key values (different keys → different S-box access patterns → different row hit rates)
- Detecting which code path was taken (branch leads to different data access sequence)

### 4.10 Python API

```python
cpu = hades.PipelinedCPU()
cpu.set_mem_hierarchy_enabled(True)   # Enable latency model

# After execution:
cpu.get_sdram_row_hits()    # Number of accesses that hit the open row
cpu.get_sdram_row_misses()  # Number of accesses that required row activation
```

### 4.11 Design Limitation

The current implementation does **not** model a separate large SDRAM address space. All addresses wrap to 64KB via `& (ONCHIP_SIZE - 1)`. The SDRAM path is a latency model only — there is no distinct data for addresses >= 0x10000. A future phase could extend `MemHierarchy` to support a larger backing store for realistic memory-mapped I/O or multi-level address spaces.

## Architecture Decision: Python Driver vs C++ Native Driver

### Why Python?

HADES uses a **C++ engine + Python driver** architecture. The engine (CPU simulation, pipeline, cache, SDRAM) is compiled C++. Python is the controller that configures, invokes, and inspects results.

This follows the same model as numpy/scipy/astropy: performance-critical inner loops in C/C++, scripting in Python for flexibility.

### The Core Tradeoff

```
Edit → Result latency   vs   Run-time performance
```

When exploring side-channel behavior, the workflow is:
1. Tweak an address or config
2. Run simulation
3. Observe timing difference
4. Repeat

With Python: edit → run (instant). With C++: edit → compile → link → run (2-5s each iteration).

The run-time performance argument is irrelevant because **Python never touches the hot loop**. When `cpu.run()` is called, execution is in compiled C++ until it returns. Python overhead is microseconds around seconds of simulation — negligible.

### Comparison

| Aspect | Python Driver | C++ Native Driver |
|--------|--------------|-------------------|
| Edit → result | Instant (no compile) | 2-5s recompile |
| Experiment scripting | 3-5 lines | 10-15 lines + recompile |
| Simulation speed | C++ (same) | C++ (same) |
| Dependencies | Python 3, pybind11 | None |
| Distribution | .so + scripts + correct Python | Single binary |
| REPL exploration | Natural (ipython, notebooks) | Not practical |
| Debugging engine | Harder (cross-language) | Single debugger session |
| Embedding in other tools | Requires Python runtime | Link as library |
| CI simplicity | Needs Python env setup | Just compile and run |

### When C++ Native Driver Makes Sense

- **Embedding**: Linking HADES into another C++ project (FPGA test harness, formal verification tool)
- **CI/CD hot path**: If test pipelines need minimal dependencies and fast startup
- **Batch benchmarking**: Running millions of configurations where Python orchestration overhead accumulates
- **Integration with C++ tooling**: Sanitizers, profilers, coverage that work better in pure C++ stacks

### Architecture: Both Coexist

```
src/
  hardware/        ← shared C++ engine (CPU, Memory, Pipeline)
  bridge/          ← pybind11 bindings (Python driver path)
  cli/             ← C++ main + encoder (native driver path, future)
    main.cpp
    encoder.h      ← instruction encoding (equivalent of riscvtools)
  demos/           ← Python demo scripts
  software/        ← Python tests
```

The Makefile supports both:
```makefile
engine:   # builds hades.so for Python driver
native:   # builds standalone hades binary (future)
```

### C++ Native Interface (Future)

A native driver would coexist with Python, sharing the same engine:

```cpp
// src/cli/main.cpp
#include "pipelined_cpu.h"
#include "encoder.h"   // C++ equivalent of riscvtools

int main(int argc, char* argv[]) {
    PipelinedCPU cpu;
    cpu.set_mem_hierarchy_enabled(true);

    auto prog = assemble({
        lui(S0, 0x10000),
        lbu(T0, S0, 0x000),
        lbu(T1, S0, 0x001),
        ecall()
    });

    cpu.load_program(prog);
    cpu.run();

    printf("Cycles: %lu\n", cpu.get_cycles());
    printf("SDRAM row hits: %lu\n", cpu.get_sdram_row_hits());
    return 0;
}
```

Use cases for the native path:
- Load and run `.bin` files compiled by the RISC-V toolchain (no encoding needed)
- Batch parameter sweeps scripted in shell
- Integration tests in CI without Python dependency
- Hot-path co-simulation where another C++ component drives the CPU in a tight loop (e.g., fuzzing instruction sequences for timing leaks)

### Decision

**Python remains the primary driver** for interactive development, demos, and tests. A C++ native driver is planned for embedding/CI scenarios but is not blocking — the engine is already cleanly separated and can be linked from either path without modification.
