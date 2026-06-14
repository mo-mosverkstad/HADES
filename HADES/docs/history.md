# HADES — Development History

---

## Phase 1: Minimal RV32I CPU + Leakage (Foundation)

### What was implemented

1. **Memory subsystem** (`engine/include/memory.h`)
   - 64KB byte-addressable memory
   - Little-endian read/write for byte, half-word, word
   - Bulk load/dump operation

2. **RV32I CPU** (`engine/include/cpu.h`, `engine/src/cpu.cpp`)
   - 32 registers (x0 hardwired to 0)
   - Program counter, cycle counter
   - Full RV32I instruction decoder (R/I/S/B/U/J formats)
   - Supported instructions:
     - Arithmetic: ADD, SUB, ADDI
     - Logic: AND, OR, XOR, ANDI, ORI, XORI
     - Comparison: SLT, SLTU, SLTI, SLTIU
     - Shift: SLL, SRL, SRA, SLLI, SRLI, SRAI
     - Load: LW, LH, LB, LHU, LBU
     - Store: SW, SH, SB
     - Branch: BEQ, BNE, BLT, BGE, BLTU, BGEU
     - Jump: JAL, JALR
     - Upper: LUI, AUIPC
     - System: ECALL (halt)
   - Leakage hook on every register write (rd != x0)

3. **pybind11 bindings** (`engine/bindings.cpp`)
   - Exposes CPU class to Python as `hades.CPU`
   - Exposes `LeakageModel` enum
   - All public methods accessible from Python

4. **Build system** (`Makefile`)
   - `make engine` — compile C++ to `build/hades.*.so`
   - `make asm` — assemble RISC-V .S files to .bin
   - `make test` — run regression tests

5. **Linker script** (`tools/programs/link.ld`)
   - Code at 0x1000, data at 0x0000, stack at 0x4000

6. **Test program** (`tools/programs/test_basic.S`)
   - Sum 1..10, XOR operation, store to memory, halt

7. **Regression tests** (`tools/test_cpu.py`)
   - 10 test cases covering all instruction categories
   - Tests leakage trace correctness (HW and HD models)
   - No external dependencies beyond `hades` module

8. **Python runner** (`tools/runner.py`)
   - CLI tool to load and execute RISC-V binaries
   - Options: dump registers, memory

---

## Phase 1.1: Project Restructuring + Assembly Workflow

### What was done

1. **Layer-aligned directory restructuring**
   - `engine/` → `layer1_hardware/` (Layers 1+2: physics + CPU simulation)
   - `engine/bindings.cpp` → `layer3_bridge/` (Layer 3: pybind11)
   - `tools/` → `layer4_software/` (Layer 4: programs for the CPU)
   - `attacks/` + `experiments/` → `layer5_attacker/` (Layer 5: attack scripts)

2. **Top-level `programs/` directory**
   - Moved assembly files out of nested `layer4_software/programs/` to top-level `programs/`
   - Easy to find, easy to add new `.S` files
   - Added `programs/README.md` with template, rules, memory map, register convention

3. **`make run-asm` convenience target**
   - One command to build and run any assembly file:
     ```bash
     make run-asm FILE=programs/my_program.S
     ```
   - Assembles → converts to binary → runs with register + trace dump

4. **Updated all documentation**
   - All paths in demos, test.md, codebase_analysis.md, history.md updated
   - Makefile rewritten for new structure
   - Import paths in Python files fixed

---

## Phase 2: 3-Stage Pipeline + Forwarding + Performance Counters

### What was implemented

1. **Pipeline register definitions** (`layer1_hardware/include/pipeline.h`)
   - StageIFID: instruction + PC + valid
   - StageEX: ALU result, rd, control flags (load/store/branch)
   - StageMEMWB: result, rd, writeback control
   - PerfCounters: mcycle, minstret, stalls_data, stalls_branch
   - CSR address constants (mcycle, minstret, mhpmcounter3-4)

2. **3-stage pipelined CPU** (`layer1_hardware/src/cpu.cpp`)
   - Pipeline stages: IF/ID → EX → MEM/WB (matches DTEK-V)
   - Back-to-front stage advancement per cycle
   - Data forwarding from EX and MEM/WB stages
   - Load-use hazard detection → 1 cycle stall (bubble insertion)
   - Branch penalty: 1 cycle (flush IF/ID on taken branch)
   - CSR read/write (CSRRW, CSRRS, CSRRC)
   - Backward-compatible single-cycle mode (`set_pipeline_enabled(False)`)

3. **Performance counters**
   - mcycle: total clock cycles
   - minstret: retired instructions
   - stalls_data: load-use hazard stalls
   - stalls_branch: branch penalty cycles
   - Accessible via `get_perf_counters()` Python API

4. **Updated bindings** (`layer3_bridge/bindings.cpp`)
   - Exposed `PerfCounters` class with readonly fields
   - Added `get_instret()`, `get_perf_counters()`, `set_pipeline_enabled()`

5. **Pipeline regression tests** (8 new tests in `layer4_software/test_cpu.py`)
   - test_pipeline_basic: correct execution in pipeline mode
   - test_pipeline_forwarding: EX→EX forwarding, 0 stalls
   - test_pipeline_load_use_stall: exactly 1 stall on load-use
   - test_pipeline_branch_penalty: branch causes ≥1 stall
   - test_pipeline_cycles_gt_instret: cycles > instructions
   - test_pipeline_loop: loop sum 1..10 = 55
   - test_perf_counters: counters accessible and non-zero
   - test_backward_compat_single_cycle: Phase 1-2 still works