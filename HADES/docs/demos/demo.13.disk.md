# Demo 13 — Block Storage Device (Disk)

This demo shows the block storage device: sector-based read/write, disk image management, and CPU access via I/O registers.

---

## Prerequisites

- WSL2 with Ubuntu 22.04+
- Python venv activated: `source ~/projects/hwsec/bin/activate`
- Engine built: `make engine`

---

## Key Concepts

- **Block device**: stores data in fixed-size sectors (512 bytes)
- **Sector**: smallest addressable unit on disk (like a "page" for storage)
- **DMA**: device copies data directly between disk and RAM (no CPU byte-by-byte)
- **Latency**: disk access is slow (10+ cycles per operation)

---

## Disk I/O Register Map (base 0xF0A0)

| Offset | Register | R/W | Description |
|--------|----------|-----|-------------|
| 0x00 | COMMAND | RW | 0=NOP, 1=READ, 2=WRITE |
| 0x04 | SECTOR | RW | Sector number to access |
| 0x08 | BUFFER | RW | RAM address for DMA transfer |
| 0x0C | STATUS | RW | 0=idle, 1=busy, 2=done, 3=error |
| 0x10 | DISK_SIZE | R | Total number of sectors |
| 0x14 | LATENCY | RW | Cycles per operation |

---

## Quick Run (One Command)

```bash
make demo-13
```

This runs `demos/demo_13_disk.py` which demonstrates disk read/write and CPU I/O access.

---

## Expected Output

```
Part 1: Write 'Hello, Disk!' to sector 0, read back -> match
Part 2: Load 4-sector image, verify all sectors, save round-trip
Part 3: CPU reads DISK_SIZE=4, STATUS=0(idle), LATENCY=10
Part 4: Modify sector 2 -> 'MODIFIED' persists
```

---

## What This Demo Proves

- Sector read/write works correctly (512-byte granularity)
- Disk image load/save round-trips perfectly
- CPU can read disk registers via memory-mapped I/O
- Sector modifications persist across reads
- All previous phases still work (disk disabled by default)