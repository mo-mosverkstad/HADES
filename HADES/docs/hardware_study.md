# HADES — Hardware Requirements Study (from DTEK-V Specs)

> Consolidated analysis of all hardware documents in `docs/hardware/`

---

## 1. Target System: DTEK-V Teaching Processor

HADES simulates a system based on the **DTEK-V** RISC-V teaching processor used in IS1200. The full system includes:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          DTEK-V System                                  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                    CPU Core (RV32IM + ZICSR)                      │  │
│  │                                                                   │  │
│  │  3-stage pipeline: IF/ID → EX → MEM/WB                            │  │
│  │  Forwarding from EX and MEM/WB                                    │  │
│  │  Performance counters (mcycle, minstret, cache misses, stalls)    │  │
│  │                                                                   │  │
│  │  ┌─────────────┐  ┌─────────────┐                                 │  │
│  │  │ L1 I-Cache  │  │ L1 D-Cache  │                                 │  │
│  │  │ 2KB, DM     │  │ 2KB, DM     │                                 │  │
│  │  │ 32B blocks  │  │ 32B blocks  │                                 │  │
│  │  │             │  │ write-thru  │                                 │  │
│  │  └──────┬──────┘  └──────┬──────┘                                 │  │
│  └─────────┼────────────────┼────────────────────────────────────────┘  │
│            │                │                                           │
│  ┌─────────▼────────────────▼────────────────────────────────────────┐  │
│  │                    Memory Bus                                     │  │
│  └──┬──────────────┬──────────────┬──────────────────────────────────┘  │
│     │              │              │                                     │
│  ┌──▼──────┐  ┌───▼────────┐  ┌─▼────────────────────────────────┐      │
│  │ On-chip │  │   SDRAM    │  │         I/O Bus (0x4000000+)     │      │
│  │ RAM     │  │ 0x00000000 │  │                                  │      │
│  │ (fast)  │  │ (64MB)     │  │  ┌─────┐ ┌──────┐ ┌─────┐        │      │
│  └─────────┘  └────────────┘  │  │Timer│ │ UART │ │GPIO │        │      │
│                               │  └─────┘ └──────┘ └─────┘        │      │
│                               │  ┌─────┐ ┌──────┐ ┌─────┐        │      │
│                               │  │Mutex│ │ VGA  │ │LEDs │        │      │
│                               │  └─────┘ └──────┘ └─────┘        │      │
│                               └──────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Map (DTEK-V)

| Address Range | Device | Latency |
|---------------|--------|---------|
| 0x00000000–0x03FFFFFF | SDRAM (64MB) | 20–50 cycles |
| 0x08000000–0x0800FFFF | On-chip RAM (64KB) | 1–2 cycles |
| 0x10000000–0x1000FFFF | Program ROM | 1 cycle |
| 0x40000000 | LEDs (output) | 1 cycle |
| 0x40000010 | Switches (input) | 1 cycle |
| 0x40000020 | Timer | 1 cycle |
| 0x40000040 | JTAG UART | 1 cycle |
| 0x400000C0 | Mutex | 1 cycle |
| 0x400000D0 | Buttons (input) | 1 cycle |
| 0x400000E0 | GPIO | 1 cycle |
| 0x40000100 | VGA (pixel buffer) | 1 cycle |

---

## 3. Device Specifications

### 3.1 Timer (0x40000020)

| Offset | Register | Bits | R/W | Description |
|--------|----------|------|-----|-------------|
| 0x00 | STATUS | [0] TO | RW | Timeout flag (write 0 to clear) |
| 0x04 | CONTROL | [0] ITO, [1] CONT, [2] START, [3] STOP | W | Control bits |
| 0x08 | PERIOD_LO | [31:0] | RW | Period low 32 bits |
| 0x0C | PERIOD_HI | [31:0] | RW | Period high 32 bits |
| 0x10 | SNAP_LO | [31:0] | R | Snapshot low (write to capture) |
| 0x14 | SNAP_HI | [31:0] | R | Snapshot high |

Behavior: Countdown timer, decrements each cycle. On zero: set TO, optionally reload (CONT), optionally IRQ (ITO).

### 3.2 JTAG UART (0x40000040)

| Offset | Register | Bits | R/W | Description |
|--------|----------|------|-----|-------------|
| 0x00 | DATA | [7:0] data, [15] RVALID, [31:16] RAVAIL | RW | Read: pop from RX FIFO. Write: push to TX FIFO |
| 0x04 | CONTROL | [0] RE, [1] WE, [8] RI, [9] WI, [10] AC, [31:16] WSPACE | RW | IRQ enables + status |

FIFO size: 64 bytes each direction.

### 3.3 GPIO (0x400000E0)

| Offset | Register | R/W | Description |
|--------|----------|-----|-------------|
| 0x00 | DATA | RW | Read: input pins. Write: output pins |
| 0x04 | DIRECTION | RW | 0=input, 1=output per bit |
| 0x08 | INTERRUPTMASK | RW | 1=enable IRQ for that bit |
| 0x0C | EDGECAPTURE | RW | 1=edge detected. Write 1 to clear |

### 3.4 Mutex (0x400000C0)

| Offset | Register | R/W | Description |
|--------|----------|-----|-------------|
| 0x00 | MUTEX | RW | [31:1] OWNER, [0] VALUE. Atomic test-and-set |
| 0x04 | RESET | W | Reset mutex to unlocked |

Lock: write OWNER+1. Read back: if OWNER matches → acquired.

### 3.5 VGA (0x40000100)

| Feature | Value |
|---------|-------|
| Resolution | 320×240 (or 640×480) |
| Color | RGB565 (16-bit) or character mode |
| Framebuffer | Memory-mapped, linear addressing |
| Character buffer | 80×60 ASCII characters |

### 3.6 On-chip RAM

- Size: 64KB
- Latency: 1–2 cycles
- Dual-port capable
- Pre-loadable (program + data)

### 3.7 SDRAM

- Size: 64MB
- Latency: 20–50 cycles (first access), 5 cycles (sequential/same row)
- Refresh: periodic stalls (~50 cycles every 10000 cycles)
- Row/bank effects: same row = fast, different row = slow

---

## 4. CPU Pipeline (3-stage)

```
┌──────────┐    ┌──────────┐    ┌──────────┐
│  IF/ID   │───▶│    EX    │───▶│  MEM/WB  │
│          │    │          │    │          │
│ Fetch +  │    │ ALU      │    │ Memory   │
│ Decode   │    │ Branch   │    │ Writeback│
└──────────┘    └──────────┘    └──────────┘
                     │                │
                     └── forwarding ──┘
```

**Hazards:**
- Data hazard: EX needs result from MEM/WB → forward or stall 1 cycle
- Load-use hazard: instruction after LOAD needs result → stall 1 cycle
- Branch: resolved in EX → 1 cycle penalty if taken

**Performance counters (CSR):**

| CSR | Name | Counts |
|-----|------|--------|
| mcycle | Cycle counter | Total cycles |
| minstret | Instruction counter | Retired instructions |
| mhpmcounter4 | I-cache misses | |
| mhpmcounter5 | D-cache misses | |
| mhpmcounter6 | I-cache stalls | |
| mhpmcounter7 | D-cache stalls | |
| mhpmcounter8 | Data hazard stalls | |
| mhpmcounter9 | ALU stalls | |

---

## 5. Cache Specification

| Parameter | I-Cache | D-Cache |
|-----------|---------|---------|
| Size | 2 KB | 2 KB |
| Mapping | Direct-mapped | Direct-mapped |
| Block size | 32 bytes | 32 bytes |
| Lines | 64 | 64 |
| Write policy | N/A | Write-through |
| Miss penalty | 20+ cycles (SDRAM) | 20+ cycles (SDRAM) |

Address breakdown (32-byte blocks, 64 lines):
```
[tag | index (6 bits) | offset (5 bits)]
index = (addr >> 5) & 0x3F
tag   = addr >> 11
```

---

## 6. Revised HADES Phase Plan (incorporating DTEK-V specs)

Phases 1–3 are completed. Remaining phases updated:

### Phase 4: 3-Stage Pipeline + Forwarding + Performance Counters

**Goal**: Match DTEK-V pipeline behavior.

**Deliverables**:
- 3-stage pipeline (IF/ID, EX, MEM/WB)
- Data forwarding from EX and MEM/WB
- Load-use hazard detection → 1 cycle stall
- Branch penalty (1 cycle if taken)
- CSR registers: mcycle, minstret
- CSR instructions: CSRRW, CSRRS, CSRRC
- Performance counters exposed to Python: `get_perf_counters()`

### Phase 5: Cache (L1 I-Cache + D-Cache)

**Goal**: Match DTEK-V cache specs.

**Deliverables**:
- L1 I-Cache: 2KB, direct-mapped, 32B blocks, 64 lines
- L1 D-Cache: 2KB, direct-mapped, 32B blocks, 64 lines, write-through
- Miss penalty configurable (default 20 cycles)
- Performance counters: mhpmcounter4–7 (cache misses/stalls)
- Cache attack: correlate AES S-box access pattern with timing

### Phase 6: Memory Hierarchy (On-chip RAM + SDRAM)

**Goal**: Realistic memory latency model.

**Deliverables**:
- On-chip RAM: 64KB, 1–2 cycle latency
- SDRAM model: row/bank effects, refresh, 20–50 cycle latency
- Address-based routing (on-chip vs SDRAM)
- Sequential access optimization (pipelined reads)

### Phase 7: I/O Devices (Timer + UART + GPIO)

**Goal**: Match DTEK-V peripheral specs.

**Deliverables**:
- Timer: countdown, continuous/one-shot, IRQ, snapshot
- JTAG UART: FIFO-based TX/RX, RVALID/RAVAIL, IRQ
- GPIO: data/direction/interruptmask/edgecapture
- Memory-mapped at DTEK-V addresses
- Interrupt controller + CPU interrupt handling (mepc, mcause, mtvec)
- Python API: `cpu.uart_send()`, `cpu.uart_recv()`, `cpu.gpio_set_input()`

### Phase 8: Mutex + Multi-core (Optional/Advanced)

**Goal**: Simulate shared-resource contention.

**Deliverables**:
- Hardware mutex with atomic test-and-set
- Dual-core simulation (2 CPU instances, shared memory)
- Contention timing observable
- Cross-core cache attacks

### Phase 9: VGA Display (Optional)

**Goal**: Visual output for debugging and demonstration.

**Deliverables**:
- Framebuffer (320×240, RGB565)
- Character buffer (80×60 ASCII)
- Memory-mapped pixel/character write
- Python-side frame capture (dump framebuffer as image)

### Phase 10: Countermeasures + Evaluation

**Goal**: Implement defenses and measure effectiveness.

**Deliverables**:
- Boolean masking
- Constant-time execution
- Cache randomization
- Noise injection
- Evaluation: attack success rate with/without countermeasures

### Phase 11: Fault Injection + DFA

**Goal**: Physical attack simulation.

**Deliverables**:
- Configurable fault: target cycle, register, memory, fault mask
- DFA on AES: correct vs faulty ciphertext → key recovery

---

## 7. HADES Simplified Memory Map (for implementation)

Since we don't need full 64MB SDRAM, HADES uses a simplified but compatible layout:

| Address Range | Device | HADES Size |
|---------------|--------|-----------|
| 0x00000000–0x0000FFFF | Main RAM (simulates SDRAM) | 64KB |
| 0x08000000–0x0800FFFF | On-chip RAM | 64KB |
| 0x10000000–0x1000FFFF | Program ROM (code loaded here) | 64KB |
| 0x40000000–0x400001FF | I/O devices | Registers |

Program entry point: 0x10000000 (or keep 0x1000 for backward compatibility with Phase 1-2).

**Decision**: For backward compatibility with existing Phase 1-2 code, keep the simplified 64KB flat memory (0x0000–0xFFFF) for now. Expand to DTEK-V full address space in Phase 6.

---

## 8. Implementation Priority

| Priority | Component | Phase | Security Value |
|----------|-----------|-------|---------------|
| HIGH | 3-stage pipeline + forwarding | 4 | Timing attacks |
| HIGH | D-Cache (2KB, DM, 32B) | 5 | Cache attacks on AES |
| HIGH | Timer | 7 | Precise timing measurement |
| MEDIUM | UART | 7 | Interactive I/O, attack sync |
| MEDIUM | SDRAM latency model | 6 | Amplifies cache miss penalty |
| MEDIUM | GPIO | 7 | Trigger, edge events |
| LOW | I-Cache | 5 | Less security-relevant |
| LOW | Mutex + multi-core | 8 | Advanced contention attacks |
| LOW | VGA | 9 | Visualization only |