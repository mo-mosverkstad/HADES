# DTEK-V Processor Information

A simplified RISC-V CPU used in IS1200 labs/projects. This is the target system modeled in HADES.

## CPU Core

### ISA Extensions

| Extension | Description |
|-----------|-------------|
| I | Base integer instructions (add, load, branch) |
| M | Multiply/divide |
| ZICSR | Control/status registers |

HADES starts with the I subset only, with optional M extension later.

### Pipeline (3-Stage)

| Stage | Function |
|-------|----------|
| 1 | IF/ID (Fetch + Decode) |
| 2 | EX (Execute) |
| 3 | MEM/WB (Memory + Writeback) |

Simpler than a typical 5-stage design while still supporting pipeline timing and hazards.

### Performance

- Clock: 30 MHz
- Peak throughput: 1 IPC

HADES tracks:
```cpp
cycles++;
instructions++;
ipc = instructions / cycles;
```

### Forwarding

Forwarding paths from Execute and MEM/WB stages:
```cpp
if (EX writes R1 and next uses R1)
    forward(EX.result);
```

Without forwarding: stalls. With forwarding: faster but still produces subtle leakage.

## Cache System

### L1 Instruction Cache

| Parameter | Value |
|-----------|-------|
| Size | 2 KB |
| Mapping | Direct-mapped |
| Block size | 32 bytes |

### L1 Data Cache

| Parameter | Value |
|-----------|-------|
| Size | 2 KB |
| Mapping | Direct-mapped |
| Block size | 32 bytes |
| Write policy | Write-through |

### HADES Cache Model

```cpp
struct CacheLine {
    bool valid;
    uint32_t tag;
};

cache[64]; // 2KB / 32B = 64 lines
```

Address breakdown:
```
index = (addr / 32) % 64
tag   = addr / (32 * 64)
```

Direct-mapped cache enables cache timing attacks, Prime+Probe, and AES cache attacks.

## Memory-Mapped I/O

### Address Space

| Range | Usage |
|-------|-------|
| 0x00000000–0x03FFFFFF | SDRAM |
| 0x40000000+ | I/O devices |

### Device Map

| Device | Address | Type |
|--------|---------|------|
| LEDs | 0x4000000 | Output |
| Switches | 0x4000010 | Input |
| Timer | 0x4000020 | Device |
| Button | 0x40000d0 | Input |
| GPIO | 0x40000e0 | I/O |

HADES hooks I/O at the memory access layer:
```cpp
if (addr >= 0x4000000) {
    io_access(addr);
}
```

## Hardware Performance Counters

| Counter | Meaning |
|---------|---------|
| mcycle | Cycle count |
| minstret | Instructions executed |
| mhpmcounter4 | I-cache misses |
| mhpmcounter5 | D-cache misses |
| mhpmcounter6 | I-cache stalls |
| mhpmcounter7 | D-cache stalls |
| mhpmcounter8 | Data hazards |
| mhpmcounter9 | ALU stalls |

HADES exposes these for leakage analysis:
```cpp
uint64_t cycle;
uint64_t instr_count;
uint64_t icache_miss;
uint64_t dcache_miss;
uint64_t stall_cycles;
```

## Exceptions

Supported:
- Instruction misaligned
- Illegal instruction
- Environment call (ecall)

## Key Architecture Insights

1. The system is CPU + cache + pipeline + I/O + counters — not just a CPU
2. All internal state (cycles, cache misses, stalls) is observable via performance counters
3. Direct-mapped cache means each address maps to exactly one cache line — vulnerable to eviction attacks
4. 3-stage pipeline provides timing variation and hazards with manageable complexity
