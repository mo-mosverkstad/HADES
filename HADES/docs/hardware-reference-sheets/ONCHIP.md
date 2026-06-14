# On-Chip Memory

FPGA on-chip memory subsystem — the fast, internal RAM/ROM used as the primary memory backing the cache.

## Key Properties

| Feature | Description |
|---------|-------------|
| Type | RAM (read/write) or ROM (read-only) |
| Access latency | 1–2 cycles |
| Ports | Single or dual-port |
| Initialization | Can be preloaded with program/data |
| Data width | Configurable (8, 16, 32 bits) |

## Memory Types

- **RAM**: Read + write
- **ROM**: Read-only (program storage, lookup tables)
- **Dual-port**: Two independent simultaneous accesses

## Latency Model

Read latency is 1 or 2 cycles depending on configuration.

```cpp
if (read_request)
    stall_for(latency);
```

Latency affects pipeline stalls, timing side-channels, and cache miss penalties.

## Dual-Port Concurrency

Two independent ports allow simultaneous access:

| Case | Result |
|------|--------|
| read + read | Safe |
| read + write (same address) | Returns old value |
| write + write (same address) | Undefined behavior |

Race conditions on write+write create observable behavior and potential attack surfaces.

## Read-During-Write Behavior

If a read and write target the same address simultaneously, the read returns the **old** data:
```cpp
old_val = mem[addr];
mem[addr] = new_val;
return old_val;
```

## Memory Initialization

Memory can be preloaded with:
- Program code
- Constants / lookup tables (e.g., AES S-box)
- Injected secrets

## Integration with Cache

```
CPU → L1 Cache → On-chip Memory
```

```cpp
if (cache_miss)
    cycles += memory_latency;
```

## HADES Implementation

```cpp
struct Memory {
    std::vector<uint8_t> data;
    int latency = 2;
};
```

External access (debug/attack):
```python
cpu.read_mem(addr)
cpu.write_mem(addr, value)
```

## Security Relevance

- **Memory timing leakage**: Access latency varies, enabling timing attacks
- **Cache interaction**: Cache miss → memory access → visible delay
- **Shared memory contention**: Two CPUs accessing simultaneously creates timing differences
- **Fault injection**: Memory corruption leads to wrong computation
