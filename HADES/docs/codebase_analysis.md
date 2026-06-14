# HADES — Codebase Analysis

---

## 0. What Is HADES? (Big Picture)

HADES is a software simulator for a hardware board with RISC-V processor. The simulator is executing on Linux or WSL, but behaves as a RISC-V processor executing machine code.

### 0.1 Real World vs HADES

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

### 0.2 Simulator Layer Diagram

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