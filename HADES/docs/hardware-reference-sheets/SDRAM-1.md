# SDRAM (External Memory)

Large, off-chip memory with higher latency than on-chip RAM. Controlled by a memory controller that abstracts the underlying SDRAM protocol.

## Position in Memory Hierarchy

```
Registers (0 cycles)
    ↓
L1 Cache (1–2 cycles)
    ↓
On-chip RAM (2–4 cycles)
    ↓
SDRAM (20–50 cycles)
```

## Key Characteristics

| Property | Description |
|----------|-------------|
| Capacity | Large (megabytes) |
| Interface | Via memory controller |
| Latency | High, variable |
| Organization | Banks, rows, columns |
| Maintenance | Requires periodic refresh |

## Latency Model

- First access (row miss): slow (~20–25 cycles)
- Sequential/same-row access: fast (~5 cycles)

```cpp
int access_latency(uint32_t addr) {
    if (same_row(addr))
        return 5;
    else
        return 25;
}
```

This data-dependent timing creates strong side-channel signal.

## Row/Bank Effects

```
Same row access → fast (row buffer hit)
Different row   → slow (row activate penalty)
```

Access pattern directly determines timing — exactly what attackers exploit.

## Refresh

SDRAM must be periodically refreshed, temporarily blocking access:

```cpp
if (cycle % refresh_interval == 0)
    stall_cycles += refresh_delay;
```

This introduces random latency spikes and realistic timing noise.

## Pipelined Reads

```
First read  = slow (full latency)
Next reads  = fast (pipelined)
```

## Wait States

The controller stalls the CPU until data is ready:
```cpp
cpu.stall(latency);
```

Directly affects execution time and leakage.

## HADES Implementation

```cpp
struct SDRAM {
    std::vector<uint8_t> data;
    int base_latency = 20;
};
```

Integration with cache:
```cpp
if (!cache_hit)
    latency = sdram_access(addr);
```

## HADES Address Routing

```cpp
if (addr in cache)
    cache_access();
else if (addr in onchip)
    fast_memory_access();
else
    sdram_access(); // slow path
```

## Security Relevance

- **Memory timing channel**: Secret → memory pattern → latency difference
- **Cache + SDRAM amplification**: Cache miss triggers slow SDRAM path → large observable delay
- **Row buffer attacks**: Same/different row access creates timing leak
- **Multi-core contention**: Shared SDRAM → interference between CPUs
