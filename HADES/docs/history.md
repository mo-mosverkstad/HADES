# HADES - Development History

---

## Phase 1: Minimal RV32I CPU (Foundation)

### What was implemented

1. **Memory subsystem** (`src/hardware/memory.h`)
   - 1MB byte-addressable memory (MemHierarchy backing store)
   - Little-endian read/write for byte, half-word, word
   - Bulk load/dump operation

2. **RV32I CPU** (`src/hardware/cpu.h`, `src/hardware/cpu.cpp`)
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

3. **pybind11 bindings** (`src/bridge/bindings.cpp`)
   - Exposes CPU class to Python as `hades.CPU`
   - All public methods accessible from Python

4. **Build system** (`Makefile`)
   - `make engine` - compile C++ to `build/hades.*.so`
   - `make asm` - assemble RISC-V .S files to .bin
   - `make test` - run regression tests

5. **Linker script** (`src/programs/link.ld`)
   - Code at 0x1000, data at 0x0000, stack at 0x4000

6. **Test program** (`src/programs/test_basic.S`)
   - Sum 1..10, XOR operation, store to memory, halt

7. **Regression tests** (`src/software/test_cpu.py`)
   - Tests covering all instruction categories
   - Tests pipeline correctness (forwarding, stalls, branches)

8. **Python runner** (`src/software/runner.py`)
   - CLI tool to load and execute RISC-V binaries
   - Options: dump registers, memory

9. **Demo** (`src/demos/demo_01_basic_cpu.py`)
   - Basic CPU execution, register/memory verification

---

## Phase 2: 3-Stage Pipeline + Forwarding + Performance Counters

### What was implemented

1. **Pipeline register definitions** (`src/hardware/pipeline.h`)
   - StageIFID: instruction + PC + valid
   - StageEX: ALU result, rd, control flags (load/store/branch)
   - StageMEMWB: result, rd, writeback control
   - PerfCounters: mcycle, minstret, stalls_data, stalls_branch

2. **3-stage pipelined CPU** (`src/hardware/pipelined_cpu.h`, `src/hardware/pipelined_cpu.cpp`)
   - Pipeline stages: IF/ID -> EX -> MEM/WB
   - Back-to-front stage advancement per cycle
   - Data forwarding from EX and MEM/WB stages
   - Load-use hazard detection -> 1 cycle stall (bubble insertion)
   - Branch penalty: 1 cycle (flush IF/ID on taken branch)
   - CSR read/write (CSRRW, CSRRS, CSRRC)

3. **Performance counters**
   - mcycle: total clock cycles
   - minstret: retired instructions
   - stalls_data: load-use hazard stalls
   - stalls_branch: branch penalty cycles
   - Accessible via `get_perf_counters()` Python API

4. **Updated bindings** (`src/bridge/bindings.cpp`)
   - Exposed `PerfCounters` class with readonly fields
   - `PipelinedCPU` class with `get_instret()`, `get_perf_counters()`

5. **Demo** (`src/demos/demo_02_pipeline.py`)
   - Pipeline execution, forwarding verification, stall counting

---

## Phase 3: Cache (L1 I-Cache + D-Cache)

### What was implemented

1. **Cache module** (`src/hardware/cache.h`)
   - Direct-mapped cache with configurable parameters
   - Read access: allocate on miss
   - Write access: write-through, no-write-allocate
   - Hit/miss counters
   - Flush operation (invalidate all lines)

2. **CPU integration**
   - L1 I-Cache: checked on every instruction fetch (pipeline mode)
   - L1 D-Cache: checked on every load/store
   - Configurable miss penalty (default 20 cycles)
   - Cache disabled by default (backward compatible)
   - `set_cache_enabled(True/False)` to toggle
   - `set_miss_penalty(cycles)` to configure

3. **Python API**
   - `cpu.set_cache_enabled(bool)` - enable/disable cache simulation
   - `cpu.set_miss_penalty(int)` - set miss penalty in cycles
   - `cpu.get_icache_misses()` - instruction cache miss count
   - `cpu.get_dcache_misses()` - data cache miss count

4. **Demo** (`src/demos/demo_03_cache.py`)
   - Cache hit/miss behavior, miss penalty impact on cycles

---

## Phase 4: Memory Hierarchy (On-chip RAM + SDRAM Model)

### What was implemented

1. **Memory hierarchy module** (`src/hardware/mem_hierarchy.h`)
   - On-chip RAM with configurable latency
   - SDRAM model with row buffer:
     - Row hit: low latency (same row as previous access)
     - Row miss: high latency (different row, must activate)
     - Periodic refresh stalls
   - Same data read/write interface

2. **CPU integration**
   - Cache miss path adds memory hierarchy latency on top of miss penalty
   - Single-cycle mode also respects memory hierarchy when enabled

3. **Python API**
   - `cpu.set_mem_hierarchy_enabled(bool)` - enable/disable
   - `cpu.get_sdram_row_hits()` - row buffer hit count
   - `cpu.get_sdram_row_misses()` - row buffer miss count

4. **Demo** (`src/demos/demo_04_memory_hierarchy.py`)
   - On-chip vs SDRAM latency comparison

---

## Phase 5: I/O Devices (Timer + UART + GPIO)

### What was implemented

1. **I/O Bus** (`src/hardware/io_bus.h`)
   - `IODevice` base class: read/write/tick/irq_pending interface
   - `IOBus` dispatcher: routes addresses >= 0xF000 to registered devices
   - Devices registered at: Timer=0xF000, UART=0xF020, GPIO=0xF040

2. **Timer** (`src/hardware/timer.h`)
   - Countdown timer
   - Registers: STATUS (TO flag), CONTROL (ITO/CONT/START/STOP), PERIOD, SNAPSHOT
   - Modes: one-shot, continuous (auto-reload)
   - IRQ on timeout (when ITO enabled)
   - Snapshot capture for safe counter read

3. **UART** (`src/hardware/uart.h`)
   - DATA register: read pops RX FIFO (with RVALID/RAVAIL), write pushes TX
   - CONTROL register: RE/WE IRQ enables, RI/WI status, WSPACE
   - 64-byte FIFO in each direction
   - Python API: `uart_send(bytes)` -> RX FIFO, `uart_recv()` -> TX output

4. **GPIO** (`src/hardware/gpio.h`)
   - Registers: DATA (read=input, write=output), DIRECTION, INTERRUPTMASK, EDGECAPTURE
   - Edge detection: captures rising/falling edges on input pins
   - IRQ when edge_capture & interrupt_mask != 0
   - Python API: `gpio_set_input(val)`, `gpio_get_output()`

5. **CPU Integration**
   - I/O address routing in memory path via MemoryPort
   - Load from I/O address -> io_bus_.read()
   - Store to I/O address -> io_bus_.write()
   - Device tick() called every CPU cycle (when I/O enabled)

6. **Demo** (`src/demos/demo_05_io_devices.py`)
   - UART echo, GPIO manipulation, Timer countdown

---

## Phase 6: Mutex + Multi-Core

### What was implemented

1. **Hardware Mutex** (`src/hardware/mutex.h`)
   - Atomic test-and-set
   - Register: [31:1] OWNER, [0] VALUE (locked/unlocked)
   - Lock: write owner+1, read back to verify ownership
   - Unlock: owner writes value=0
   - Contention counter (tracks failed lock attempts)

2. **Multi-Core Controller** (`src/hardware/multicore.h`)
   - 2 CPU cores with independent registers, PC
   - Shared memory (MemHierarchy)
   - Shared I/O bus (Timer, UART, GPIO, Mutex)
   - Round-robin execution (Core 0 step, Core 1 step, tick devices)
   - Terminates when both cores halt (ECALL)

3. **Python API** (via `hades.MultiCore`)
   - `load_program(core_id, binary, base_addr)` - load per-core program
   - `load_data(data, base_addr)` - load shared data
   - `run(max_cycles)` - execute both cores
   - `get_reg(core_id, idx)` - per-core register read
   - `get_cycles(core_id)` / `get_instret(core_id)` - per-core counters
   - `get_global_cycles()` - total system cycles
   - `get_mutex_contentions()` / `get_mutex_locked()` / `get_mutex_owner()`
   - Shared UART/GPIO access

4. **Demo** (`src/demos/demo_06_multicore.py`)
   - Part 1: Independent execution (both cores compute, store to shared memory)
   - Part 2: Mutex contention (Core 0 locks, Core 1 gets contention)

---

## Phase 7: VGA Display

### What was implemented

1. **VGA Device** (`src/hardware/vga.h`)
   - Pixel buffer: 320x240, RGB565 (16-bit per pixel)
   - Character buffer: 80x60, ASCII (8-bit per character)
   - Color buffer: 80x60, per-character foreground color
   - Registers: CONTROL (mode/enable), CURSOR_X/Y, PIXEL_ADDR, PIXEL_DATA, CHAR_WRITE
   - Auto-increment: pixel address advances on write, cursor advances on char write
   - VSYNC simulation
   - Memory-mapped at 0xF080

2. **Python API**
   - `cpu.vga_get_framebuffer()` -> list of 76800 uint16 (320x240 RGB565)
   - `cpu.vga_get_char_buffer()` -> list of 4800 uint8 (80x60 ASCII)
   - `cpu.vga_get_color_buffer()` -> list of 4800 uint8 (80x60 color)
   - `cpu.vga_get_char_row(row)` -> string (80 chars)

3. **Demo** (`src/demos/demo_07_vga.py`)
   - Part 1: Character mode - CPU writes text, Python reads back
   - Part 2: Pixel mode - CPU writes RGB565 colors
   - Part 3: Multi-line debug output

---

## Phase 8: Interactive Game (Full System Demo)

### What was implemented

1. **"Guess the Number" game assembly** (`src/programs/guess_game.S`)
   - RISC-V assembly program
   - Reads player guesses from UART (keyboard simulation)
   - Displays prompts and feedback on VGA character buffer
   - Game logic: compare guess with secret, show LOW/HIGH/WIN!
   - Signals win via UART TX ('W')

2. **Demo 08: Auto-play with Python encoder** (`src/demos/demo_08_game.py`)
   - Builds game as machine code using Python encoder functions
   - No cross-compiler needed (self-contained)
   - Python plays automatically using binary search
   - Verifies VGA output and UART TX win signal
   - Command: `make demo-08`

3. **Demo 08b: Auto-play with real toolchain** (`src/demos/demo_08b_game_asm.py`)
   - Assembles `src/programs/guess_game.S` using `riscv64-unknown-elf-gcc`
   - Converts to raw binary with `objcopy`
   - Loads binary into HADES and plays automatically
   - Command: `make demo-08b`

4. **Demo 08c: Interactive keyboard play** (`src/demos/demo_08c_game_interactive.py`)
   - Assembles `src/programs/guess_game.S` using real gcc
   - Player types guesses from the keyboard
   - Command: `make demo-08c`

5. **Demo 08d: Interactive with color** (`src/demos/demo_08d_game_interactive.py`)
   - Enhanced interactive variant
   - Command: `make demo-08d`

6. **Demonstrates full system integration**
   - CPU: executes branches, loops, comparisons
   - UART: bidirectional I/O (input guesses, output win signal)
   - VGA: character display (game UI)
   - Real toolchain: same .S file works with gcc and on HADES

---

## Phases 9-10: Internal Architecture Refactoring

Phase 9 introduced threaded processor execution (allowing the CPU to run
independently of Python's call loop). Phase 10 extracted the internal
Executor pattern and restored the CRTP base class (`cpu_base.h`) to
eliminate code duplication between `CPU` and `PipelinedCPU`. These were
structural changes with no new demos - all existing demos produce
identical output.

---

## Phase 11: Echo Terminal (Real-Time I/O) + RISC-V Tools

### What was implemented

1. **riscvtools package** (`src/riscvtools/`)
   - `asmpack.py`: Python-based RV32I instruction encoder (encode_u, encode_i, encode_s, encode_b, encode_r, encode_j), register constants, opcode constants
   - `assemblyloader.py`: checks toolchain, assembles .S -> .elf -> .bin, loads into CPU
   - `terminaldisplay.py`: reads VGA char+color buffers, renders with ANSI escape codes
   - `terminalinput.py`: reads keyboard, converts to UART bytes, sends to CPU
   - Shared by demo_08b, demo_08c, demo_08d, demo_11, demo_13

2. **Echo terminal assembly** (`src/programs/echo_terminal.S`)
   - Pure RISC-V assembly
   - Main loop: spin on UART RX -> echo to UART TX -> display on VGA
   - Cursor management: advance on character, newline on Enter
   - Backspace support
   - Color commands ($r=red, $g=green, $b=blue, $y=yellow, $c=cyan, $m=magenta)
   - $q to quit (halt CPU)

3. **Interactive demo** (`src/demos/demo_11_echo_terminal.py`)
   - Assembles `src/programs/echo_terminal.S` with AssemblyLoader
   - Interactive loop: keyboard -> UART -> CPU -> VGA -> ANSI terminal display
   - Uses TerminalDisplay and TerminalInput from riscvtools
   - Command: `make demo-11`

---

## Phase 12: MMU / Virtual Memory (Sv32)

### What was implemented

1. **MMU module** (`src/hardware/mmu.h`)
   - RISC-V Sv32 two-level page table walker
   - TLB: 16-entry fully-associative with LRU replacement
   - Page Table Entry format: PPN(22) + flags (V/R/W/X/U/G/A/D)
   - SATP CSR integration (MODE + PPN)
   - Page fault detection (load/store/instruction faults)
   - Permission checking (R/W/X per page)
   - TLB flush on SATP write

2. **Memory integration** (`src/hardware/memory.h`)
   - MemoryPort translates via MMU before accessing backing store
   - Separate instruction and data ports (imem/dmem)
   - Faults reported per-port

3. **Python API**
   - `cpu.set_mmu_satp(satp)` - enable MMU and set page table base
   - `cpu.get_mmu_satp()` - read current SATP
   - `cpu.mmu_flush_tlb()` - invalidate all TLB entries
   - `cpu.get_tlb_hits()` / `cpu.get_tlb_misses()` / `cpu.get_page_faults()`

4. **Demo** (`src/demos/demo_12_mmu.py`)
   - Sets up two-level Sv32 page tables in physical memory
   - Maps virtual pages to physical frames with permissions
   - Enables MMU via SATP
   - Verifies translation and TLB/page fault statistics
   - Command: `make demo-12`

---

## Phase 13: Block Storage Device (Disk)

### What was implemented

1. **Block device module** (`src/hardware/block_device.h`)
   - 512-byte sectors, configurable disk size (default 128 sectors = 64KB)
   - I/O registers at 0xF0A0: COMMAND, SECTOR, BUFFER, STATUS, DISK_SIZE, LATENCY
   - Commands: READ (disk -> RAM via DMA), WRITE (RAM -> disk via DMA)
   - DMA transfer: device copies 512 bytes between disk and CPU RAM
   - Configurable latency (simulates seek/rotational delay)
   - IRQ on completion (optional)

2. **DMA wiring** (`src/hardware/cpu_base.h`)
   - Disk DMA callbacks connected to CPU data memory port
   - CPU can issue disk read/write commands via I/O registers
   - Transfer happens immediately on COMMAND write

3. **Python API**
   - `cpu.disk_load_image(bytes)` - load disk image
   - `cpu.disk_save_image()` - get disk contents
   - `cpu.disk_write_sector(sector, data)` - direct sector write
   - `cpu.disk_read_sector(sector)` - direct sector read
   - `cpu.get_disk_reads()` / `cpu.get_disk_writes()` - operation counts

4. **Demo** (`src/demos/demo_13_disk.py`)
   - Part 1: Write/read sector round-trip (host-side)
   - Part 2: Disk image load/save (multi-sector)
   - Part 3: CPU reads disk registers via I/O (size, status, latency)
   - Part 4: Sector modification + statistics
   - Command: `make demo-13`
