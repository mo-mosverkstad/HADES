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


## Phase 5: I/O Devices (Timer + UART + GPIO)

### 5.1 I/O Bus Architecture (`layer1_hardware/include/io_bus.h`)

Memory-mapped I/O uses the same load/store instructions as RAM access. The CPU doesn't know it's talking to a device — the I/O bus intercepts addresses >= 0xF000:

```
CPU executes: SW t0, 0(t3)    where t3 = 0xF020
    │
    ├─ addr < 0xF000? → normal memory (RAM/cache)
    │
    └─ addr >= 0xF000? → I/O Bus dispatches to device
                              │
                              ├─ 0xF000-0xF01F → Timer
                              ├─ 0xF020-0xF03F → UART
                              └─ 0xF040-0xF05F → GPIO
```

The `IODevice` base class defines the interface all devices must implement:

```cpp
class IODevice {
public:
    virtual uint32_t read(uint32_t offset) = 0;   // CPU reads register
    virtual void write(uint32_t offset, uint32_t value) = 0; // CPU writes register
    virtual void tick() = 0;           // called every CPU cycle
    virtual bool irq_pending() = 0;    // device wants to interrupt
};
```

### 5.2 Timer (`layer1_hardware/include/timer.h`)

Matches DTEK-V interval timer specification.

**Register map** (offsets from 0xF000):

| Offset | Register | Function |
|--------|----------|----------|
| 0x00 | STATUS | bit 0: TO (timeout occurred). Write 0 to clear. |
| 0x04 | CONTROL | bit 0: ITO (IRQ enable), bit 1: CONT, bit 2: START, bit 3: STOP |
| 0x08 | PERIOD_LO | Countdown period (low 32 bits) |
| 0x0C | PERIOD_HI | Countdown period (high 32 bits) |
| 0x10 | SNAP_LO | Write triggers snapshot capture; read returns captured value |

**Behavior:**
```
Each CPU cycle (tick()):
    if running and counter > 0:
        counter--
    if counter == 0:
        set TO flag
        if CONT: reload counter from period
        else: stop
```

### 5.3 UART (`layer1_hardware/include/uart.h`)

Matches DTEK-V JTAG UART specification. Provides bidirectional communication between CPU and Python (host).

**Register map** (offsets from 0xF020):

| Offset | Register | Read | Write |
|--------|----------|------|-------|
| 0x00 | DATA | [7:0] byte, [15] RVALID, [31:16] RAVAIL | [7:0] byte to TX |
| 0x04 | CONTROL | [0] RE, [1] WE, [8] RI, [9] WI, [10] AC, [31:16] WSPACE | [0] RE, [1] WE |

**Data flow:**
```
Python (host)                          CPU (simulated)
     │                                      │
     │  cpu.uart_send([0x48, 0x69])         │
     │  ─────────────────────────────►      │
     │         (bytes go into RX FIFO)      │
     │                                      │  LW t0, 0(uart_addr)  → pops 0x48
     │                                      │  LW t1, 0(uart_addr)  → pops 0x69
     │                                      │
     │                                      │  SW t0, 0(uart_addr)  → pushes 0x48 to TX
     │  output = cpu.uart_recv()            │
     │  ◄─────────────────────────────      │
     │         (reads TX output buffer)     │
```

### 5.4 GPIO (`layer1_hardware/include/gpio.h`)

Matches DTEK-V PIO (Parallel I/O) specification.

**Register map** (offsets from 0xF040):

| Offset | Register | Function |
|--------|----------|----------|
| 0x00 | DATA | Read: input pin values. Write: output pin values. |
| 0x04 | DIRECTION | 0=input, 1=output per bit |
| 0x08 | INTERRUPTMASK | 1=enable IRQ for that bit |
| 0x0C | EDGECAPTURE | 1=edge detected. Write 1 to clear. |

**Edge detection:**
```cpp
void GPIO::tick() {
    uint32_t changed = input_pins_ ^ prev_input_;
    edge_capture_ |= changed;  // latch any transitions
    prev_input_ = input_pins_;
}
```

**Security relevance:**
- GPIO toggle used as oscilloscope trigger (marks crypto start/end in trace)
- Edge capture reveals timing of external events
- In real attacks: attacker toggles GPIO pin → CPU starts AES → attacker captures power trace synchronized to the trigger

### 5.5 CPU Integration

The I/O bus is checked in both pipeline and single-cycle memory access paths:

```cpp
// In stage_memory() and execute_single_cycle():
if (io_enabled_ && io_bus_.is_io_address(addr)) {
    // Route to I/O device instead of RAM
    result = io_bus_.read(addr);   // for loads
    io_bus_.write(addr, value);    // for stores
} else {
    // Normal memory path (cache → memory hierarchy)
}
```

Devices are ticked every CPU cycle:
```cpp
void CPU::pipeline_cycle() {
    perf_.mcycle++;
    if (io_enabled_) io_bus_.tick_all();  // all devices advance one cycle
    ...
}
```

I/O is auto-enabled when `uart_send()` or `gpio_set_input()` is called from Python, ensuring zero overhead when I/O is not used.

### 5.6 Demo: `demo_07_io_devices.py`

Demonstrates all three devices in one script:

| Part | Device | What happens | Verification |
|------|--------|-------------|-------------|
| 1 | UART | Python sends 'H','i' → CPU reads and echoes → Python receives [72,105] | Bidirectional FIFO works |
| 2 | GPIO | Python sets input=0xAA → CPU XORs with 0xFF → output=0x55 | Read/write routing correct |
| 3 | Timer | Set period=100, start, do work, read snapshot | Countdown decrements |

Run: `make demo-07`

### 5.7 Full System with I/O (Updated Diagram)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ demos/                                                                  │
│  demo_01 → demo_07                                                      │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ import hades
┌────────────────────────────────────▼────────────────────────────────────┐
│                        C++ Engine (build/hades.*.so)                    │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │ CPU Core (3-stage pipeline)                                      │   │
│  │   IF/ID → EX → MEM/WB                                            │   │
│  │   forwarding, hazard detection                                   │   │
│  └──────────────────────────┬───────────────────────────────────────┘   │
│                             │ memory access                             │
│                             ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │ Address Router                                                   │   │
│  │   addr < 0xF000? ──► Cache ──► Memory Hierarchy (RAM/SDRAM)      │   │
│  │   addr >= 0xF000? ──► I/O Bus                                    │   │
│  └──────────────────────────┬───────────────────────────────────────┘   │
│                             │                                           │
│            ┌────────────────┼────────────────┐                          │
│            ▼                ▼                ▼                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │
│  │    Timer     │  │    UART      │  │    GPIO      │                   │
│  │   0xF000     │  │   0xF020     │  │   0xF040     │                   │
│  │  countdown   │  │  FIFO TX/RX  │  │  pins I/O    │                   │
│  │  IRQ on TO   │  │  host ↔ CPU  │  │  edge detect │                   │
│  └──────────────┘  └──────────────┘  └──────────────┘                   │
│            │                │                │                          │
│            └────────────────┼────────────────┘                          │
│                             │ irq_pending()                             │
│                             ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │ Leakage Engine (records power on every register write)           │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Python API:                                                            │
│    cpu.uart_send(bytes)     → push to RX FIFO                           │
│    cpu.uart_recv()          → read TX output                            │
│    cpu.gpio_set_input(val)  → set input pins                            │
│    cpu.gpio_get_output()    → read output pins                          │
└─────────────────────────────────────────────────────────────────────────┘
```

## Phase 6: Multi-Core + Mutex

### 6.1 Hardware Mutex (`layer1_hardware/include/mutex.h`)

Implements DTEK-V hardware mutex with atomic test-and-set semantics. Beware that the mutex relies entirely on software cooperation and does not prevent a core from directly reading or writing shared memory. Programs are expected to acquire the mutex before accessing shared data and release it afterward. If a program ignores the mutex protocol, the hardware will not stop it, and race conditions can still occur.

**Register** (at 0xF060):

| Bits | Field | Meaning |
|------|-------|---------|
| [0] | VALUE | 0=unlocked, 1=locked |
| [31:1] | OWNER | ID of the core that holds the lock |

**Atomic lock acquisition:**
```cpp
void Mutex::write(uint32_t offset, uint32_t value) {
    uint32_t new_owner = value >> 1;
    bool new_value = (value & 1) != 0;

    if (new_value) {
        // Lock attempt
        if (!locked_) {
            locked_ = true;          // free → acquired
            owner_ = new_owner;
        } else if (owner_ != new_owner) {
            contention_count_++;     // another core holds it → FAIL
        }
    } else {
        // Unlock attempt (only owner can unlock)
        if (locked_ && owner_ == new_owner) {
            locked_ = false;
            owner_ = 0;
        }
    }
}
```

Key property: the write is **atomic** — only one core can succeed per cycle. The other gets a contention (write has no effect).

### 6.2 Multi-Core Controller (`layer1_hardware/include/multicore.h`)

Manages two CPU cores sharing a single memory and I/O bus.

**Architecture:**
```
┌─────────────────────────────────────────────────────────────────┐
│                    MultiCore Controller                         │
│                                                                 │
│  ┌──────────────────┐          ┌──────────────────┐             │
│  │     Core 0       │          │     Core 1       │             │
│  │  regs[32], PC    │          │  regs[32], PC    │             │
│  │  I-cache, D-cache│          │  I-cache, D-cache│             │
│  │  starts at 0x1000│          │  starts at 0x2000│             │
│  └────────┬─────────┘          └─────────┬────────┘             │
│           │                              │                      │
│           └──────────────┬───────────────┘                      │
│                          │ shared bus                           │
│           ┌──────────────▼───────────────┐                      │
│           │      Shared Resources        │                      │
│           │                              │                      │
│           │  ┌────────────────────────┐  │                      │
│           │  │ Memory (64KB shared)   │  │                      │
│           │  └────────────────────────┘  │                      │
│           │                              │                      │
│           │  ┌──────┐ ┌────┐ ┌────┐      │                      │
│           │  │Timer │ │UART│ │GPIO│      │                      │
│           │  └──────┘ └────┘ └────┘      │                      │
│           │                              │                      │
│           │  ┌────────────────────────┐  │                      │
│           │  │ MUTEX (0xF060)         │  │                      │
│           │  │ atomic test-and-set    │  │                      │
│           │  └────────────────────────┘  │                      │
│           └──────────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Execution model** (round-robin):
```cpp
void MultiCore::run(uint32_t max_cycles) {
    for (uint32_t c = 0; c < max_cycles; c++) {
        global_cycle_++;
        if (!cores_[0].halted) step_core(0);  // Core 0 executes 1 instruction
        if (!cores_[1].halted) step_core(1);  // Core 1 executes 1 instruction
        io_bus_.tick_all();                   // Devices advance 1 cycle
        if (cores_[0].halted && cores_[1].halted) break;
    }
}
```

Each core has its own `CoreState` with independent:
- Register file (32 * uint32)
- Program counter
- L1 I-cache and D-cache
- Cycle and instruction counters

But they share:
- Memory (both read/write the same 64KB)
- I/O devices (Timer, UART, GPIO, Mutex)

### 6.3 Mutex Protocol (Software Side)

To acquire the lock, a core writes its ID and value=1:
```asm
# Core 0 (owner=1): lock
li   t0, 3          # (owner=1)<<1 | (value=1) = 3
sw   t0, 0(mutex_addr)

# Core 0: unlock
li   t0, 2          # (owner=1)<<1 | (value=0) = 2
sw   t0, 0(mutex_addr)

# Core 1 (owner=2): try lock
li   t0, 5          # (owner=2)<<1 | (value=1) = 5
sw   t0, 0(mutex_addr)
lw   t1, 0(mutex_addr)  # read back: if owner=2 → success
```

### 6.4 Contention Side-Channel

The key security insight demonstrated in this phase:

```
Core 0: lock -> [crypto operation: N cycles] -> unlock
Core 1: try_lock -> FAIL -> spin -> try_lock -> ... -> SUCCESS

Core 1's total spin time = N cycles = execution time of Core 0's crypto
```

**Observable from Python:**
```python
mc.get_global_cycles()       # increases with Core 0's work
mc.get_mutex_contentions()   # counts failed lock attempts
```

**Experimental result from demo:**
```
Core 0 work =  5 NOPs -> global_cycles = 12
Core 0 work = 15 NOPs -> global_cycles = 22
Difference: 10 cycles = exactly 10 extra NOPs
```

This proves: **lock hold time is directly measurable by the contending core**, creating a timing side-channel that requires NO power measurement — just cycle counting.

### 6.5 Demo: `demo_06_multicore.py`

| Part | What happens | Key observation |
|------|-------------|----------------|
| 1 | Two cores compute independently | Both results correct in shared memory |
| 2 | Core 0 locks, Core 1 gets contention | 2 contentions counted, mutex state visible |
| 3 | Variable work under lock | 5 NOPs=12 cycles, 15 NOPs=22 cycles (linear!) |

Run: `make demo-06`

### 6.6 Updated Full System Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│ demos/                                                                  │
│  demo_01 -> demo_06                                                     │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────────────┐
│                     Python API                                          │
│                                                                         │
│  hades.CPU         (single-core, pipeline, cache, mem hierarchy, I/O)   │
│  hades.MultiCore   (dual-core, shared memory, mutex, round-robin)       │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────────────┐
│                     C++ Engine (build/hades.*.so)                       │
│                                                                         │
│  Single-core path:                                                      │
│    CPU → Pipeline -> Cache -> MemHierarchy -> I/O Bus                   │
│                                                                         │
│  Multi-core path:                                                       │
│    MultiCore -> Core0.step() + Core1.step() -> shared Memory + I/O      │
│                                             -> Mutex (atomic lock)      │
│                                                                         │
│  Leakage: per-core power trace (independent)                            │
└─────────────────────────────────────────────────────────────────────────┘
```

## Phase 7: VGA Display

### 7.1 VGA Device (`layer1_hardware/include/vga.h`)

The VGA provides visual output from the CPU, supporting two modes:

**Character mode** (80*60 ASCII):
- Fast text display, like a terminal
- Cursor auto-advances on each character write
- Useful for printf-style debugging

**Pixel mode** (320*240 RGB565):
- 16-bit color per pixel (5 red, 6 green, 5 blue)
- Address auto-increments on each pixel write
- Useful for graphics, plots, visualizations

### 7.2 Register Map (base 0xF080)

| Offset | Register | Read | Write |
|--------|----------|------|-------|
| 0x00 | CONTROL | mode + enable flags | [0] mode (0=pixel,1=char), [1] enable |
| 0x04 | STATUS | [0] VSYNC | (read-only) |
| 0x08 | CURSOR_X | current column | set column (0-79) |
| 0x0C | CURSOR_Y | current row | set row (0-59) |
| 0x10 | PIXEL_ADDR | current pixel address | set address (0-76799) |
| 0x14 | PIXEL_DATA | read pixel at addr | write RGB565, auto-increment addr |
| 0x18 | CHAR_WRITE | read char at cursor | write ASCII, auto-advance cursor |

### 7.3 Character Buffer Operation

```cpp
void VGA::write(uint32_t offset, uint32_t value) {
    case 0x18: // CHAR_WRITE
        chars_[cursor_y_ * 80 + cursor_x_] = (uint8_t)(value & 0x7F);
        cursor_x_++;
        if (cursor_x_ >= 80) {
            cursor_x_ = 0;
            cursor_y_++;  // wrap to next line
        }
}
```

CPU writes characters one at a time. The cursor advances automatically, wrapping at end of line. This mimics how real terminal hardware works — the CPU just sends bytes, the display handles positioning.

### 7.4 Pixel Buffer Operation

```cpp
void VGA::write(uint32_t offset, uint32_t value) {
    case 0x14: // PIXEL_DATA
        pixels_[pixel_addr_] = (uint16_t)(value & 0xFFFF);
        pixel_addr_++;  // auto-increment for sequential fills
}
```

RGB565 color format:
```
Bit:  15 14 13 12 11 | 10 9 8 7 6 5 | 4 3 2 1 0
      R  R  R  R  R  | G  G G G G G | B B B B B

Red   = 0xF800 = 11111_000000_00000
Green = 0x07E0 = 00000_111111_00000
Blue  = 0x001F = 00000_000000_11111
White = 0xFFFF = 11111_111111_11111
Black = 0x0000 = 00000_000000_00000
```

### 7.5 Python API

```python
# Read entire framebuffer (76800 uint16 values)
fb = cpu.vga_get_framebuffer()
pixel_at_0_0 = fb[0]
pixel_at_x_y = fb[y * 320 + x]

# Read character buffer
chars = cpu.vga_get_char_buffer()  # 4800 uint8 values

# Read a single row as string (convenient for text verification)
row0 = cpu.vga_get_char_row(0)  # "HADES..."
```

### 7.6 Typical Usage Pattern (Assembly)

```asm
# Write "OK" to VGA character display
    li   t4, 0xF080        # VGA base address
    li   t0, 3             # CONTROL = char_mode(1) | enable(2)
    sw   t0, 0x00(t4)     # set control
    sw   zero, 0x08(t4)   # cursor_x = 0
    sw   zero, 0x0C(t4)   # cursor_y = 0
    li   t0, 'O'
    sw   t0, 0x18(t4)     # write 'O', cursor advances
    li   t0, 'K'
    sw   t0, 0x18(t4)     # write 'K', cursor advances
```

### 7.8 Demo: `demo_07_vga.py`

| Part | Mode | What CPU writes | Python reads back |
|------|------|----------------|-------------------|
| 1 | Character | 'HADES' at (0,0) | `vga_get_char_row(0)` -> "HADES" |
| 2 | Pixel | 5 RGB565 colors | `vga_get_framebuffer()[0:5]` -> [0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000] |
| 3 | Character | 'OK' row 0, '42' row 1 | Multi-line readback verified |

Run: `make demo-07`

### 7.9 Complete I/O Device Map (All Phases)

```
┌─────────────────────────────────────────────────────────────────┐
│                    I/O Address Space (0xF000+)                  │
├──────────┬──────────────────────────────────────────────────────┤
│ 0xF000   │ Timer    (countdown, IRQ, snapshot)         Phase 7  │
│ 0xF020   │ UART     (FIFO TX/RX, host<->CPU)           Phase 7  │
│ 0xF040   │ GPIO     (pins, edge detect, IRQ)           Phase 7  │
│ 0xF060   │ Mutex    (atomic lock, multi-core)          Phase 8  │
│ 0xF080   │ VGA      (320*240 pixel + 80*60 char)       Phase 9  │
└──────────┴──────────────────────────────────────────────────────┘
```

All devices share the same interface pattern:
1. CPU does `SW value, offset(base_addr)` -> device register write
2. CPU does `LW rd, offset(base_addr)` -> device register read
3. I/O bus routes based on address range
4. Python can interact via dedicated APIs (`uart_send`, `gpio_set_input`, `vga_get_*`)

--

## Phase 8: Interactive Game (Full System Integration)

### 8.1 Purpose

Phase 8 proves HADES is a **complete embedded system** which can run interactive programs with real-time I/O.

### 8.2 Game: "Guess the Number"

A simple game where:
- CPU picks a secret number (1-9, hardcoded as 5)
- Player sends ASCII digit guesses via UART (keyboard simulation)
- CPU compares guess with secret
- VGA displays feedback: "LOW", "HIGH", or "WIN!"
- On win: CPU sends 'W' via UART TX and halts

### 8.3 I/O Flow

```
┌──────────────────┐         ┌──────────────────────────────────────┐
│  Player          │         │  HADES CPU                           │
│  (Python/human)  │         │                                      │
│                  │         │  ┌────────────────────────────────┐  │
│  Type '3' ───────┼── UART ─┼─▶│ Read UART DATA register        │  │
│                  │   RX    │  │ Check RVALID bit               │  │
│                  │         │  │ Extract ASCII byte             │  │
│                  │         │  │ Convert to number (- '0')      │  │
│                  │         │  │ Compare with secret            │  │
│                  │         │  │                                │  │
│                  │         │  │ Write result to VGA:           │  │
│  See ">3 LOW" ◀─┼── VGA ──┼──│  ">3 LOW" / ">3 HIGH" / "WIN!" │  │
│                  │  chars  │  └────────────────────────────────┘  │
│                  │         │                                      │
│  Detect win ◀────┼── UART ─┼── Send 'W' on win                   │
│                  │   TX    │                                      │
└──────────────────┘         └──────────────────────────────────────┘
```

### 8.4 Assembly Implementation (`programs/guess_game.S`)

Key structure (72 instructions, 288 bytes):

```asm
_start:
    # Setup UART (0xF020) and VGA (0xF080) addresses
    # Init VGA: char mode, display "GUESS 1-9"
    # Set secret = 5

game_loop:
    lw   t0, 0(uart)        # read UART DATA
    srli t1, t0, 15         # check RVALID (bit 15)
    beqz t1, game_loop      # spin if no data

    andi t2, t0, 0xFF       # extract ASCII byte
    addi t3, t2, -48        # convert to number

    beq  t3, s0, win        # guess == secret?
    blt  t3, s0, too_low    # guess < secret?
    # ... display "HIGH" on VGA ...
    j    next_round

too_low:
    # ... display "LOW" on VGA ...
    j    next_round

win:
    # ... display "WIN!" on VGA ...
    # ... send 'W' to UART TX ...
    ecall                    # halt
```

### 8.5 Three Demo Variants

| Demo | Command | Input source | Toolchain needed |
|------|---------|-------------|-----------------|
| demo_08 | `make demo-08` | Python auto-plays (binary search) | None (Python encodes machine code) |
| demo_08b | `make demo-08b` | Python auto-plays | `riscv64-unknown-elf-gcc` |
| demo_08c | `make demo-08c` | **Keyboard (interactive)** | `riscv64-unknown-elf-gcc` |

All three run the same game logic. The difference is:
- **demo_08**: machine code generated by Python encoder functions (self-contained)
- **demo_08b**: `.S` file assembled by real gcc toolchain, auto-played by Python
- **demo_08c**: `.S` file assembled by real gcc toolchain, **you play via keyboard**

### 8.6 Interactive Session (demo_08c)

```
$ make demo-08c

============================================================
DEMO 08c: Interactive 'Guess the Number'
  (assembled from programs/guess_game.S, 288 bytes)
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

  Final VGA Display:
  ┌────────────────────┐
  │ GUESS 1-9          │
  │ >3 LOW             │
  │ >7 HIGH            │
  │ >5 WIN!            │
  └────────────────────┘

  Total CPU cycles: 103
```

### 8.7 How Interactive Mode Works

Each iteration of the Python loop:

```python
# 1. Read keyboard input
user_input = input("Your guess (1-9): ")

# 2. Send to CPU via UART (simulates keyboard → serial port)
cpu.uart_send([ord(user_input)])

# 3. Run CPU (processes guess, writes to VGA, spins waiting for next)
cpu.run(10000)

# 4. Read VGA to show CPU's response
row_text = cpu.vga_get_char_row(guess_num)
print(f"CPU says: {row_text}")

# 5. Check UART TX for win signal
tx = cpu.uart_recv()
if ord('W') in tx:
    won = True
```

The CPU program runs in a loop:
- Spins on UART read (waiting for data)
- When data arrives: processes, writes VGA, loops back to spin
- On win: writes UART TX and halts (ECALL)

Python feeds one byte at a time, runs the CPU until it spins again, then reads the result. This creates a turn-based interactive loop.

### 8.8 What This Proves

| Capability | Demonstrated |
|-----------|-------------|
| CPU executes loops + branches | Game loop with conditional logic |
| UART input (keyboard) | Player sends guesses |
| UART output (signaling) | CPU signals win to host |
| VGA output (display) | Game UI rendered by CPU |
| Real toolchain compatibility | Same .S file works with gcc |
| Interactive I/O | Human-in-the-loop execution |
| Complete embedded system | All components working together |

This is the culmination of all 8 phases: a fully functional embedded computer running an interactive program, with observable side-channels at every level.

---

### 8.9 Bug Fix: Demo 08c Unresponsive Processor

**Commit:** `c903da2` — "HADES: Phase 8 - Fix demo8c unresponsible processor bug"  
**File changed:** `src/hardware/pipelined_cpu.cpp`

### Symptom

In `demo_08c_game_interactive.py`, the PipelinedCPU would only respond to the **first** user input. All subsequent inputs produced empty output:

```
  Your guess (1-9): 3
  PipelinedCPU says: >3 LOW
  Your guess (1-9): 3
  PipelinedCPU says: 
  Your guess (1-9): ^C
```

The CPU appeared completely frozen after the first guess, regardless of how long the user waited between inputs.

### Root Cause

The `PipelinedCPU::run()` method used **absolute** comparisons against cumulative performance counters:

```cpp
// BUGGY — absolute thresholds
void PipelinedCPU::run(uint32_t max_instructions) {
    uint64_t max_cycles = (uint64_t)max_instructions * 4;
    while (perf_.mcycle < max_cycles) {          // absolute cycle limit
        pipeline_cycle();
        if (halted_ && !memwb_.valid) break;
        if (perf_.minstret >= max_instructions) break;  // absolute instruction limit
    }
}
```

The counters `perf_.mcycle` and `perf_.minstret` are **cumulative** — they monotonically increase across all calls to `run()` and are never reset between calls.

The interactive demo calls `run()` multiple times in sequence:

1. `cpu.run(5000)` — display title. After this: `minstret ≈ 4500`.
2. User types '3'. `cpu.run(10000)` — process first guess. After this: `minstret ≈ 11000`.
3. User types '3' again. `cpu.run(10000)` — **immediately exits** because `minstret (11000) >= max_instructions (10000)` is already true on the very first iteration.

The cycle check (`perf_.mcycle < 40000`) was also problematic but the instruction check triggered first due to the ratio of spinning instructions in the UART polling loop.

### Why Waiting Doesn't Help

The CPU does **not** run in real-time. It only executes when Python explicitly calls `cpu.run(N)`. Between keypresses, the CPU is completely frozen. The real-world clock is irrelevant — only the cumulative instruction counter matters, and it was already past the threshold.

### The Fix

Changed to **relative** comparisons — snapshot the counters at the start of each `run()` call and measure progress from that baseline:

```cpp
// FIXED — relative thresholds
void PipelinedCPU::run(uint32_t max_instructions) {
    uint64_t start_cycles = perf_.mcycle;
    uint64_t start_instret = perf_.minstret;
    uint64_t max_cycles = (uint64_t)max_instructions * 4;
    while (perf_.mcycle - start_cycles < max_cycles) {       // relative
        pipeline_cycle();
        if (halted_ && !memwb_.valid) break;
        if (perf_.minstret - start_instret >= max_instructions) break;  // relative
    }
}
```

Now each `run(N)` call executes **up to N instructions from wherever the CPU currently is**, regardless of how many instructions were executed in previous calls.

### Analogy

- **Bug:** "Run until the odometer shows 10,000 km." Works for the first trip, but after a round trip the odometer already reads 12,000, so the next "run 10,000" does nothing.
- **Fix:** "Run for 10,000 km from the current reading." Works every time.

### Impact

This bug affected **any** code pattern that calls `cpu.run()` multiple times on the same CPU instance — not just the interactive demo. The chunk-based execution used by CERBERUS (`cpu.run(500)` in a loop) also relies on correct relative behavior.

### Verification

After the fix, piped input produces correct output for all guesses:

```
$ echo -e "3\n7\n5" | make demo-08c
  Your guess (1-9): PipelinedCPU says: >3 LOW
  Your guess (1-9): PipelinedCPU says: >7 HIGH
  Your guess (1-9): PipelinedCPU says: >5 WIN!
  You won in 3 guesses!
```

---

## Phase 9. Threaded Processor Execution

### 9.1 Motivation

The current execution model is **synchronous and cooperative**: the CPU only runs when Python explicitly calls `cpu.run(N)`, executes exactly N instructions, then returns control. This creates a stop-and-go pattern:

```
Python: get user input → cpu.uart_send() → cpu.run(10000) → cpu.vga_get_char_row() → ...
```

This has fundamental limitations:

1. **The CPU cannot run independently.** It must be manually stepped by the host program. A real processor runs continuously until it halts or is interrupted.
2. **Budget estimation.** The caller must guess how many cycles are "enough" to process input. Too few → incomplete processing. Too many → wasted time in a spin loop.
3. **No concurrent I/O.** The host cannot read UART output while the CPU is running. It must wait for `run()` to return, then check.
4. **Interactive demos are fragile.** The demo-08c fix showed that repeated `run(N)` calls require careful budget management.

### 9.2 Design: Threaded Execution Model

#### Core Idea

Move CPU execution to a **dedicated background thread**. The CPU runs continuously (like real hardware) while Python interacts with I/O devices concurrently from the main thread.

#### Thread Lifecycle

```
                         ┌──────────────────────────────┐
                         │   CPU Execution Thread       │
                         │                              │
   run(0) ──────────────►│  while (!halted && !stop):   │
                         │      pipeline_cycle()        │
                         │                              │
   run(N) ──────────────►│  for i in 0..N:             │
                         │      pipeline_cycle()        │
                         │  then: pause (idle)          │
                         │                              │
   stop() ──────────────►│  sets stop flag → exits loop │
                         │  thread idles (not killed)   │
                         └──────────────────────────────┘
```

#### API Semantics (Backward Compatible)

| Call | Current Behavior | New Behavior |
|------|-----------------|--------------|
| `cpu.run(10000)` | Execute 10000 instructions, block until done | Start thread if not started. Execute 10000 instructions, block until done. Returns after completion. |
| `cpu.run(0)` | Execute 0 instructions (no-op) | Start thread if not started. **Run indefinitely** until halt or `stop()`. Non-blocking: returns immediately. |
| `cpu.run()` (default=1000000) | Execute 1M instructions | Same as current: execute up to 1M instructions, block until done. |

The key insight: **`run(N)` with N>0 remains blocking and backward-compatible.** Only `run(0)` introduces new free-running behavior.

#### New Methods

| Method | Purpose |
|--------|---------|
| `cpu.stop()` | Force the CPU to pause execution. Sets a stop flag that the execution loop checks every cycle. Does **not** reset state — PC, registers, memory all preserved. |
| `cpu.is_running()` | Returns `true` if the CPU thread is actively executing (not paused, not halted). |
| `cpu.wait()` | Block until the CPU halts or pauses (useful after `run(0)` to wait for halt). |

#### Thread Safety for I/O

UART, GPIO, and VGA must be **thread-safe** since Python reads/writes them from the main thread while the CPU thread accesses them every cycle:

| Device | Shared State | Synchronization |
|--------|-------------|-----------------|
| UART RX FIFO | Host pushes, CPU pops | Mutex or lock-free queue |
| UART TX buffer | CPU pushes, host pops | Mutex or lock-free queue |
| GPIO input/output | Host writes input, CPU writes output | Atomic uint32_t |
| VGA char/frame buffer | CPU writes, host reads | Copy-on-read or mutex |
| Timer | CPU reads/ticks | No sharing needed (CPU-only) |

#### Stop Mechanism (Dead Loop Safety)

If `run(0)` is called and the program enters a dead loop (no halt, no I/O progress):

```python
cpu.run(0)                  # start free-running
time.sleep(5)               # wait for some reasonable time
if cpu.is_running():        # still going?
    cpu.stop()              # force pause
    # CPU is now paused at whatever PC it reached
    # State is preserved — can inspect registers, memory
    # Can call run() again to resume
```

The stop flag is checked every cycle (`if (stop_requested_) break;`), so worst case latency is one pipeline cycle.

### 9.3 How C++ Threading Works (Concepts)

This section explains the C++ threading primitives used in HADES, for those unfamiliar with `<thread>`, `<mutex>`, and `<condition_variable>`.

#### What Is a Thread?

A thread is a separate flow of execution running **in parallel** with your main program. In C++, you create one with `std::thread`:

```cpp
std::thread t(some_function, arg1, arg2);
// 'some_function' now runs concurrently in a new OS thread
```

The thread starts immediately upon construction. The main thread continues executing the next line while `some_function` runs in parallel.

**Critical rule:** Before a `std::thread` object is destroyed, you must call either:
- `t.join()` — block until the thread finishes
- `t.detach()` — let it run independently (rarely what you want)

If you destroy a joinable thread without doing either, the program calls `std::terminate()` (crashes).

#### std::mutex — Protecting Shared Data

When two threads access the same variable, you get **data races** (undefined behavior). A mutex (mutual exclusion) prevents this:

```cpp
std::mutex mtx;
int shared_counter = 0;

// Thread 1:                    // Thread 2:
mtx.lock();                     mtx.lock();        // blocks until Thread 1 unlocks
shared_counter++;               shared_counter++;
mtx.unlock();                   mtx.unlock();
```

Only one thread can hold the lock at a time. The other thread **blocks** (sleeps) until the lock is released.

**RAII wrappers** (preferred — exception-safe, can't forget to unlock):

```cpp
{
    std::lock_guard<std::mutex> lk(mtx);  // locks on construction
    shared_counter++;
}   // automatically unlocks when 'lk' goes out of scope
```

`std::unique_lock` is like `lock_guard` but can be unlocked/relocked manually — required for condition variables.

#### std::condition_variable — Signaling Between Threads

A condition variable lets one thread **sleep** until another thread **wakes it up**. This is how the CPU thread sleeps between `run()` calls instead of busy-spinning.

Pattern (producer/consumer):

```cpp
std::mutex mtx;
std::condition_variable cv;
bool ready = false;   // the "condition" being waited on

// WAITING thread (CPU thread):
{
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&]{ return ready; });  // sleeps until ready == true
    // woken up, lock is held, ready is guaranteed true
    ready = false;  // reset for next time
}

// SIGNALING thread (Python/main thread):
{
    std::lock_guard<std::mutex> lk(mtx);
    ready = true;
    cv.notify_one();  // wake up the waiting thread
}
```

**How `cv.wait(lk, predicate)` works internally:**
1. Check predicate — if already true, don't sleep, just return
2. If false: atomically **release the mutex** and put this thread to sleep
3. When `notify_one()` is called: wake up, **re-acquire the mutex**, check predicate again
4. If predicate is true: return (thread continues with lock held)
5. If predicate is false (spurious wakeup): go back to sleep

The predicate lambda (`[&]{ return ready; }`) prevents **spurious wakeups** — the OS can wake threads for no reason, so you must always re-check the condition.

#### std::atomic — Lock-Free Shared Variables

For simple flags/counters, `std::atomic` provides thread-safe access without a mutex:

```cpp
std::atomic<bool> stop_requested_{false};

// Thread 1 (writer):
stop_requested_ = true;    // atomic store

// Thread 2 (reader, every cycle):
if (stop_requested_) break;   // atomic load
```

Atomics are much cheaper than mutexes (no OS call, no blocking), but only work for single-variable operations. You can't atomically update *two* variables with atomics alone.

#### How These Connect in HADES

Here's the complete lifecycle of a `run(N)` call:

```
Python calls cpu.run(10000)
        │
        ▼
┌─ Main Thread ─────────────────────────┐    ┌─ CPU Thread ──────────────────────────┐
│                                       │    │                                       │
│ 1. First call? Create thread:         │    │ (thread starts here)                  │
│    exec_thread_ = std::thread(...)    │───►│                                       │
│                                       │    │ 2. wait_for_run_signal():             │
│ 3. signal_run(10000):                 │    │    cv.wait(lk, []{return signaled;})  │
│    lock mutex                         │    │    ... sleeping ...                   │
│    budget_ = 10000                    │    │                                       │
│    run_signaled_ = true               │    │                                       │
│    cv.notify_one()  ─────────────────────► │ 4. Wakes up! budget_ = 10000          │
│    unlock mutex                       │    │    running_ = true                    │
│                                       │    │                                       │
│ 5. wait_for_completion():             │    │ 6. Loop: pipeline_cycle() * 10000     │
│    cv.wait(lk, []{return done;})      │    │    (checks stop_requested_ each cycle)│
│    ... sleeping ...                   │    │                                       │
│                                       │    │ 7. Done! running_ = false             │
│                                       │  ◄─── notify_completion():                 │
│ 8. Wakes up! Returns to Python.       │    │    done_signaled_ = true              │
│                                       │    │    cv.notify_one()                    │
│                                       │    │                                       │
│                                       │    │ 9. Loop back to wait_for_run_signal() │
│                                       │    │    ... sleeping until next run() ...  │
└───────────────────────────────────────┘    └───────────────────────────────────────┘
```

For `run(0)` (free-running), step 5 is skipped — main thread returns immediately, and the CPU thread runs until `halted_` or `stop()`.

#### Destructor — Clean Shutdown

When the Python object is garbage-collected, the C++ destructor must stop the thread:

```cpp
~CPUBase() {
    if (thread_started_) {
        stop_requested_ = true;  // break out of run_pipeline if mid-execution
        {
            std::lock_guard<std::mutex> lk(run_mutex_);
            shutdown_ = true;       // tell thread to exit
            run_signaled_ = true;   // wake it from sleep
            run_cv_.notify_one();
        }
        exec_thread_.join();  // wait for thread to actually finish
    }
}
```

Without this, destroying the object while the thread sleeps causes a crash (`std::terminate`). The `shutdown_` flag is checked in `thread_main()` right after waking:

```cpp
void PipelinedCPU::thread_main() {
    while (true) {
        wait_for_run_signal();
        if (shutdown_) return;  // exit the thread function → thread dies
        // ... normal execution ...
    }
}
```

#### Summary of Primitives

| Primitive | Purpose in HADES | Cost |
|-----------|-----------------|------|
| `std::thread` | Runs `pipeline_cycle()` loop independently | One OS thread |
| `std::mutex` + `condition_variable` | Main↔CPU thread signaling (run/done) | Cheap when uncontended |
| `std::atomic<bool>` | `stop_requested_`, `running_` flags | Nearly free (single atomic load/store) |
| `std::lock_guard` | RAII mutex lock for signal_run/notify | Auto-unlock on scope exit |
| `std::unique_lock` | Required by `cv.wait()` (must be unlockable) | Same as lock_guard + flexibility |

### 9.4 Implementation Plan

#### Phase A: Thread Infrastructure

```cpp
// Added to CPUBase<T>
protected:
    std::thread exec_thread_;
    std::atomic<bool> running_{false};     // thread is actively executing
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> thread_started_{false};
    bool shutdown_ = false;
    std::mutex run_mutex_;
    std::condition_variable run_cv_;
    std::condition_variable done_cv_;
    uint64_t budget_ = 0;  // 0 = free-running (infinite)
    bool run_signaled_ = false;
    bool done_signaled_ = false;

    void signal_run(uint64_t n) {
        std::lock_guard<std::mutex> lk(run_mutex_);
        budget_ = n;
        run_signaled_ = true;
        run_cv_.notify_one();
    }

    void wait_for_run_signal() {
        std::unique_lock<std::mutex> lk(run_mutex_);
        run_cv_.wait(lk, [this]{ return run_signaled_; });
        run_signaled_ = false;
    }

    void notify_completion() {
        std::lock_guard<std::mutex> lk(run_mutex_);
        done_signaled_ = true;
        done_cv_.notify_one();
    }

    void wait_for_completion() {
        std::unique_lock<std::mutex> lk(run_mutex_);
        done_cv_.wait(lk, [this]{ return done_signaled_; });
        done_signaled_ = false;
    }
```

Destructor ensures clean shutdown:

```cpp
~CPUBase() {
    if (thread_started_) {
        stop_requested_ = true;  // break out of run_pipeline if mid-execution
        {
            std::lock_guard<std::mutex> lk(run_mutex_);
            shutdown_ = true;
            run_signaled_ = true;
            run_cv_.notify_one();
        }
        exec_thread_.join();
    }
}
```

#### Phase B: Modify `run()` Semantics

```cpp
void PipelinedCPU::run(uint32_t max_instructions) {
    if (running_) return;  // already executing — ignore re-entrant call
    if (!thread_started_) {
        exec_thread_ = std::thread(&PipelinedCPU::thread_main, this);
        thread_started_ = true;
    }
    stop_requested_ = false;
    if (max_instructions == 0) {
        // Free-running mode: signal thread to run indefinitely, return immediately
        signal_run(0);
        return;
    }
    // Bounded mode: signal thread to run N instructions, block until done
    signal_run(max_instructions);
    wait_for_completion();
}
```

#### Phase C: Thread Main Loop

```cpp
void PipelinedCPU::thread_main() {
    while (true) {
        wait_for_run_signal();  // sleep until run() is called
        if (shutdown_) return;
        running_ = true;
        run_pipeline(budget_, [&](){
            return stop_requested_.load(std::memory_order_relaxed);
        });
        running_ = false;
        notify_completion();  // wake up blocking run(N) caller if any
    }
}

template<typename Predicate>
void PipelinedCPU::run_pipeline(uint64_t max_instructions, Predicate check_stop) {
    uint64_t start_cycles = perf_.mcycle;
    uint64_t start_instret = perf_.minstret;
    uint64_t max_cycles = max_instructions * 4;
    while (max_instructions == 0 || perf_.mcycle - start_cycles < max_cycles) {
        if (check_stop()) break;
        pipeline_cycle();
        if (halted_ && !memwb_.valid) break;
        if (max_instructions != 0 && perf_.minstret - start_instret >= max_instructions) break;
    }
}
```

#### Phase D: Thread-Safe I/O

Replace UART queues with lock-free or mutex-protected variants:

```cpp
class UART : public IODevice {
    std::mutex rx_mutex_;
    std::mutex tx_mutex_;
    // ...
    void host_send(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        for (uint8_t b : data) rx_fifo_.push(b);
    }
    uint32_t read(uint32_t offset) override {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        // pop from rx_fifo_
    }
};
```

GPIO uses `std::atomic<uint32_t>` — no mutex needed.

VGA uses a mutex on the character/framebuffer, or provides a snapshot method.

### 9.5 Revised Interactive Demo Pattern

With threading, the interactive demo becomes much simpler:

```python
cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.load_program(binary, 0x1000)
cpu.run(0)  # start free-running in background

while not won:
    user_input = input("Your guess (1-9): ")
    cpu.uart_send([ord(user_input)])
    time.sleep(0.01)  # let CPU process
    row_text = cpu.vga_get_char_row(guess_num)
    print(f"CPU says: {row_text}")
```

No more guessing cycle budgets. The CPU runs at full speed, processes UART input as soon as it arrives, and the host reads VGA output whenever it wants.

### 9.6 Edge Cases and Safety

| Scenario | Handling |
|----------|----------|
| `run(0)` then `run(N)` | Second `run()` calls `stop()` implicitly, then starts bounded execution |
| `run(N)` then `run(0)` | First completes (blocks), then second starts free-running |
| `stop()` while paused | No-op |
| `reset()` while running | Must `stop()` first, then reset state |
| Destructor called while thread running | `stop()` + `join()` in destructor |
| `run(0)` and program halts naturally | Thread detects `halted_`, sets `running_ = false`, idles |

### 9.7 Backward Compatibility Guarantee

| Existing code pattern | Still works? | Why |
|----------------------|:------------:|-----|
| `cpu.run(10000)` | ✓ | Blocking, executes exactly N instructions |
| `cpu.run()` (default 1M) | ✓ | Blocking, up to 1M instructions |
| Multiple `cpu.run(N)` calls | ✓ | Each blocks and executes N more instructions |
| `cpu.uart_send()` before `run()` | ✓ | Data sits in FIFO, processed when CPU runs |
| `cpu.uart_recv()` after `run()` | ✓ | Returns whatever CPU wrote to TX buffer |

All existing demos and the CERBERUS `run.py` chunk-execution pattern continue to work unchanged.

### 9.8 Performance Considerations

- **Mutex cost per cycle:** If UART/VGA are protected by mutex, every `pipeline_cycle()` that does a load/store to I/O space acquires a lock. For the hot polling loop in `guess_game.S` (reads UART every ~4 cycles), this adds overhead.
- **Mitigation:** Use lock-free SPSC (single-producer single-consumer) queues for UART FIFOs. CPU is the only consumer of RX and only producer of TX. Host is the opposite. No mutex needed.
- **VGA:** Reads are infrequent from Python side. A simple mutex is fine — only acquired when Python calls `vga_get_char_row()`.
- **GPIO:** Atomic operations — zero overhead beyond the atomic load/store.

### 9.9 Summary

| Aspect | Current | Threaded |
|--------|---------|----------|
| CPU execution | On-demand, Python-driven | Continuous, independent thread |
| Blocking model | Always blocking | `run(N>0)` blocking, `run(0)` non-blocking |
| Dead loop handling | Stuck forever in `run()` | `stop()` breaks out within 1 cycle |
| I/O timing | Only during `run()` windows | Anytime (concurrent) |
| Cycle budget guessing | Required | Optional (can use `run(0)`) |
| API compatibility | — | 100% backward compatible |

---

---

## Phase 10. Architecture: Internal Executor Pattern

### 10.1 Refactoring Journey

The threading architecture went through three iterations before arriving at the current design. Each step revealed a problem that the next step solved.

#### Attempt 1: Threading in CPUBase (Active Object)

The original approach embedded all threading infrastructure directly into the CRTP base class:

```
CPUBase<T>
├── std::thread exec_thread_
├── std::mutex run_mutex_
├── std::condition_variable run_cv_, done_cv_
├── signal_run(), wait_for_completion(), ...
├── CPU state (regs_, pc_, mem_)
├── I/O devices (uart_, gpio_, vga_)
│
├── CPU (single-cycle)      ← inherits ALL threading, never uses it
└── PipelinedCPU            ← uses threading, run() spawns thread
```

**Problems discovered:**
- `CPU` carried dead threading weight (thread was never started, but destructor still checked)
- `MultiCore` was a completely separate class duplicating everything
- No way to test pipeline logic without spawning threads
- Thread-safety was purely "by convention" — no structural enforcement
- The `thread_main()` infinite loop needed a `shutdown_` flag, and forgetting it caused hangs on object destruction

#### Attempt 2: Executor<T> as Wrapper (External Executor)

Next, we extracted all threading into a generic `Executor<T>` template that *owned* the model:

```
Executor<PipelinedCPU>  ← user-facing type
    │
    └── PipelinedCPU     ← hidden inside, pure computation
```

Python bindings exposed `Executor<PipelinedCPU>` as `"PipelinedCPU"`.

**Problems discovered:**
- **C++ API break:** Users who previously wrote `PipelinedCPU cpu;` now needed `Executor<PipelinedCPU> cpu;`
- **Wrapper explosion:** Every new method on the model required a forwarding method on Executor
- **Decoration fragility:** Adding any cross-cutting concern (tracing, power model, etc.) would require yet another wrapper layer, each breaking the API
- **Type leakage:** If someone wrapped `PipelinedCPU` in C++ for their own purposes, adding threading later would change the type they instantiate

**Key insight:** Making Executor the user-facing type means any architectural change (adding/removing/restructuring the Executor) is an API break by definition.

#### Attempt 3: Internal Executor (Final Design)

The solution: invert the ownership. The model class IS the API and owns the Executor privately:

```
PipelinedCPU  ← user-facing type (forever)
    │
    └── Executor exec_{...}  ← private member, never exposed
```

**Why this works:**
- Class name `PipelinedCPU` never changes
- Adding threading = adding `run(0)` + `stop()` (additive, no break)
- Removing or replacing Executor = invisible (it's private)
- No forwarding needed (model has the methods directly)
- Works identically in C++ and Python

### 10.2 API Stability Rule

**Formal rule: Public class interfaces must only grow. They must never shrink or change.**

This means:

| Allowed | Forbidden |
|---------|-----------|
| Add new public method | Remove existing public method |
| Add new optional parameter with default | Change existing parameter type |
| Add new class | Rename existing class |
| Change private/internal implementation | Change public method signature |
| Make sync method also support async | Make previously sync method always async |

**Practical enforcement:**

1. **The public type IS the API.** `PipelinedCPU`, `CPU`, `MultiCore` — these names are permanent. Internal helpers (`Executor`, `IOBus`, `StageIFID`) can be renamed, restructured, or removed freely.

2. **`run(N)` contract:**
   - `run(N>0)` = synchronous, blocks until done (always, forever)
   - `run(0)` = async, returns immediately (new behavior, additive)
   - Default argument `run()` = `run(1000000)` = synchronous

3. **New capabilities are additive methods:**
   - Want threading? Add `stop()`, `is_running()`. Don't change `run(N)` for N>0.
   - Want power trace? Add `get_power_trace()`. Don't modify `get_cycles()`.
   - Want watchpoints? Add `set_breakpoint()`. Don't change `step()`.

4. **Internal restructuring is free:**
   - Replace `std::mutex` in UART with lock-free queue → zero API change
   - Change pipeline from 3-stage to 5-stage → zero API change
   - Replace Executor with a coroutine-based scheduler → zero API change

**Why this matters for HADES:**
The simulator is used from Python demos, C++ unit tests, and potentially future wrappers (e.g., a GUI, a fuzzer, a power analysis framework). If adding threading breaks C++ callers, or adding power tracing breaks Python demos, the project becomes unmaintainable. The "only grow" rule prevents this class of bug entirely.

### 10.3 Architecture: Executor as Internal Detail

The Executor is **owned by** the model class, not the other way around. Users never see it.

```
What the user writes (C++ or Python) — NEVER CHANGES:

    PipelinedCPU cpu;
    cpu.load_program(binary);
    cpu.run(1000);          // synchronous
    cpu.run(0);             // async (new capability, same method)
    cpu.stop();             // new method (additive)
    cpu.get_reg(5);
    cpu.get_perf_counters();
```

Internal structure:

```
┌─────────────────────────────────────────────────────────────┐
│               PipelinedCPU  (THE public API)                 │
│                                                              │
│  PUBLIC (stable contract, only grows):                        │
│    run(N)  — N>0: sync, N==0: async (via Executor)           │
│    stop()  — halt async execution                            │
│    is_running() — query async state                          │
│    load_program(), get_reg(), get_pc(), get_cycles()         │
│    uart_send(), vga_get_char_row(), gpio_set_input() ...     │
│                                                              │
│  PRIVATE (free to refactor):                                 │
│    Executor exec_{...};   ← internal, never exposed          │
│    regs_[], pc_, pipeline stages, memory, I/O devices        │
│    step() — called by Executor AND by sync run()             │
└─────────────────────────────────────────────────────────────┘
```

### 10.4 Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Python / pybind11 Bindings                            │
│                                                                              │
│   hades.CPU            →  py::class_<CPU>                                    │
│   hades.PipelinedCPU   →  py::class_<PipelinedCPU>                           │
│   hades.MultiCore      →  py::class_<MultiCore>                              │
│                                                                              │
│   (Executor is NEVER mentioned in bindings)                                  │
└──────────────────────────────────────────────────────────────────────────────┘
            │                      │                      │
            ▼                      ▼                      ▼
┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐
│       CPU         │  │   PipelinedCPU    │  │    MultiCore      │
│ (public class)    │  │ (public class)    │  │ (public class)    │
│                   │  │                   │  │                   │
│ run(N):           │  │ run(N):           │  │ run(N):           │
│  N>0 → sync loop  │  │  N>0 → sync loop  │  │  (sync only for   │
│  N=0 → exec_      │  │  N=0 → exec_      │  │   now)            │
│       .run_async() │  │       .run_async() │  │                   │
│                   │  │                   │  │                   │
│ PRIVATE:          │  │ PRIVATE:          │  │ PRIVATE:          │
│  Executor exec_   │  │  Executor exec_   │  │  CoreState[2]     │
│  step()           │  │  step()           │  │  shared memory    │
│  regs_, pc_       │  │  pipeline stages  │  │                   │
│  Memory, I/O      │  │  Memory, I/O      │  │                   │
└───────────────────┘  └───────────────────┘  └───────────────────┘
         │                      │
         └──────────┬───────────┘
                    ▼
      ┌─────────────────────────────────────┐
      │      Thread-Safe I/O Devices         │
      │                                     │
      │  UART  — mutex on rx_fifo/tx_output │
      │  GPIO  — std::atomic<uint32_t>      │
      │  VGA   — mutex on chars[]/pixels[]  │
      │  Timer — CPU-thread-only (no share) │
      └─────────────────────────────────────┘
```

### 10.5 Executor Class (Internal Utility)

```cpp
// executor.h — NOT part of public API. Internal implementation detail.
class Executor {
public:
    using StepFn = std::function<void()>;
    using HaltedFn = std::function<bool()>;

    Executor(StepFn step, HaltedFn halted);
    ~Executor();  // stop + join

    void run_async(uint64_t budget);  // 0 = infinite
    void wait();                       // block until done
    void stop();                       // set stop flag
    bool is_running() const;

private:
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_run_, cv_done_;
    // ... signaling state ...

    void thread_main();  // loop: wait → step N times → notify
};
```

**When `~Executor()` is called:**

`Executor` is a private member of `CPU` and `PipelinedCPU`. Neither class declares an explicit destructor. When the owning object is destroyed (C++ RAII scope exit, or Python's garbage collector releasing the pybind11 handle → `delete`), the compiler-generated destructor destroys all members in reverse declaration order. This triggers `~Executor()`, which:

1. Sets `stop_requested_ = true` (breaks out of any active step loop)
2. Sets `shutdown_ = true` and signals `cv_run_` (wakes the sleeping thread)
3. Calls `thread_.join()` (blocks until the background thread exits)

This guarantees the background thread is fully stopped before any other member (`mem_`, `uart_`, etc.) is destroyed — no dangling references.

Usage inside a model class:

```cpp
class PipelinedCPU {
    // ...
private:
    Executor exec_{
        [this]{ step(); },          // what to execute each cycle
        [this]{ return is_halted(); } // when to auto-stop
    };
};
```

### 10.6 run() Semantics

```cpp
void PipelinedCPU::run(uint32_t max_instructions) {
    if (max_instructions == 0) {
        // Async: start background thread, return immediately
        exec_.run_async(0);
        return;
    }
    // Sync: execute directly on calling thread (no Executor involved)
    for (...) { step(); }
}
```

**Key insight:** `run(N)` with N>0 **never touches the Executor**. It's a plain loop.
Only `run(0)` engages the thread. This means:
- All existing demos (which use `run(N)`) have zero threading overhead
- Threading is only activated for interactive/free-running use cases

### 10.7 Thread Ownership Diagram

```
MAIN THREAD (Python/C++ user)            EXECUTOR THREAD (background)
═══════════════════════════════           ════════════════════════════

cpu.load_program(bin)   ─── (no thread exists yet) ───
cpu.run(10000)          ─── sync loop on main thread, NO thread spawned ───
cpu.get_reg(5)          ─── direct read, safe ───

cpu.run(0)              ─── exec_.run_async(0) ─────────► thread spawns/wakes
   (returns immediately)                                   step() step() step()
cpu.uart_send([0x41])   ─── thread-safe (UART mutex) ──   step() step() ...
cpu.vga_get_char_row()  ─── thread-safe (VGA mutex) ───   step() step() ...
cpu.stop()              ─── sets atomic flag ──────────►  checks → breaks
                                                           (thread sleeps)
cpu.get_reg(5)          ─── safe (thread is sleeping) ──
cpu.run(5000)           ─── sync loop on main thread ───  (thread still sleeping)
```

### 10.8 API Stability Guarantee

| Scenario | API impact |
|----------|-----------|
| Add threading support | `run(0)` + `stop()` added. Existing `run(N)` unchanged. |
| Add power trace leakage model | New methods added (`get_power_trace()`). Nothing removed. |
| Replace UART with lock-free queue | Zero API change. Internal optimization. |
| Add new I/O device | New methods added (`spi_send()`). Nothing removed. |
| Refactor Executor internals | Zero API change. Users never see Executor. |
| Switch pipeline from 3-stage to 5-stage | Zero API change. `step()` internals differ. |

**The rule is simple: public class interfaces only grow. Internal classes can be freely restructured.**

### 10.9 Thread-Safe I/O Device Design

```
┌──────────────────────────────────────────┐
│              UART (IODevice)              │
├──────────────────────────────────────────┤
│ Called by CPU thread (step() path):      │
│   read(0x00)  ← locks rx_mutex_, pops   │
│   write(0x00) ← locks tx_mutex_, pushes  │
├──────────────────────────────────────────┤
│ Called by main thread (Python API):      │
│   host_send() ← locks rx_mutex_, pushes  │
│   host_recv() ← locks tx_mutex_, swaps   │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│              GPIO (IODevice)             │
├──────────────────────────────────────────┤
│ CPU thread:  read()  ← atomic load      │
│              write() ← atomic store      │
├──────────────────────────────────────────┤
│ Main thread: set_input() ← atomic store │
│              get_output() ← atomic load  │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│              VGA (IODevice)              │
├──────────────────────────────────────────┤
│ CPU thread:  write(0x18) ← char_mutex_  │
│              write(0x14) ← pixel_mutex_  │
├──────────────────────────────────────────┤
│ Main thread: get_char_row() ← char_mutex_│
│              get_framebuffer()← pixel_mu │
└──────────────────────────────────────────┘
```

### 10.10 File Structure (Final)

```
src/hardware/
├── cpu_base.h          ← CRTP base class: shared state, I/O, accessors
├── executor.h          ← Internal thread utility (NOT public API)
├── cpu.h / cpu.cpp     ← Single-cycle model (derives CPUBase, owns Executor)
├── pipelined_cpu.h/cpp ← Pipeline model (derives CPUBase, owns Executor)
├── multicore.h         ← Multi-core model (sync only, for now)
├── pipeline.h          ← Pipeline stage structs, PerfCounters
├── rv32_decode.h       ← Instruction decoder
├── memory.h            ← Memory hierarchy
├── io_bus.h            ← I/O dispatch
├── uart.h              ← Thread-safe UART
├── gpio.h              ← Thread-safe GPIO (atomic)
├── vga.h               ← Thread-safe VGA
├── timer.h             ← Timer (CPU-only, no sharing)
├── cache.h             ← Cache model
├── mem_hierarchy.h     ← SDRAM + on-chip RAM
└── mutex.h             ← Hardware mutex (for MultiCore)

src/bridge/
└── bindings.cpp        ← pybind11: binds CPU, PipelinedCPU, MultiCore directly

```

### 10.11 Design Rules

| Rule | Rationale |
|------|-----------|
| Public classes ARE the API | Users write `PipelinedCPU cpu;` — this never changes |
| Executor is private/internal | Can be refactored/replaced without affecting users |
| Methods only added, never removed | Backward compatibility guaranteed |
| `run(N>0)` = sync, no thread | Zero overhead for non-interactive use |
| `run(0)` = async via internal Executor | Thread only spawned when actually needed |
| I/O devices self-protect | Mutexes/atomics inside the device, not outside |
| pybind binds model classes directly | No wrapper types leak into Python |

---

## Phase 10.1. Restore CRTP Base Class (`cpu_base.h`)

### Problem

During Phase 10 (Internal Executor Pattern), the refactoring inadvertently removed `cpu_base.h` — the CRTP base class that eliminated code duplication between `CPU` and `PipelinedCPU`. The Executor extraction was correct, but the refactoring had the unintended side effect of inlining all shared state and methods back into both derived classes independently, creating ~40 lines of duplicated declarations per class.

### What was lost

`CPUBase<Derived>` provided:
- Shared state: `regs_[32]`, `pc_`, `halted_`, `io_enabled_`, `io_bus_`, `mem_`, `timer_`, `uart_`, `gpio_`, `vga_`
- Shared public methods: `load_program()`, `load_data()`, `get_pc()`, `get_reg()`, `read_mem()`, `is_halted()`, all cache/memory config, all I/O accessors
- Shared protected: `write_reg()`, `reset_base()`

### Fix applied

Restored `cpu_base.h` with the CRTP pattern, adapted to coexist with the Executor pattern:

```
CPUBase<Derived>           ← CRTP template: shared state + methods
  ├─ CPU                   ← adds: cycles_, Executor, step(), execute()
  └─ PipelinedCPU         ← adds: pipeline stages, perf counters, CSRs, Executor
```

The `Executor` remains in each derived class (not in the base) because its lambdas capture the derived `this` pointer. This is correct — Executor is an internal implementation detail of each model's threading strategy.

### Key design point

`CPUBase<T>` provides a `reset_base()` protected method that both derived classes call from their `reset()`. This eliminates duplicated I/O device registration code (`io_bus_.register_device(...)`) that was previously copy-pasted in both `CPU::reset()` and `PipelinedCPU::reset()`.

### Verification

All 18 regression tests pass. All demos (01–08c) produce identical output to pre-fix code. Zero behavioral change — purely structural.
