# Demo 12 - MMU / Virtual Memory (Sv32)

This demo shows the HADES MMU hardware: setting up Sv32 page tables, enabling translation via SATP CSR, and verifying the page table structure.

---

## Prerequisites

- WSL2 with Ubuntu 22.04+
- Python venv activated: `source ~/projects/hwsec/bin/activate`
- Engine built: `make engine`

---

## Key Concepts

- **Sv32**: RISC-V 32-bit virtual memory scheme (two-level page tables, 4KB pages)
- **SATP CSR**: System register that enables MMU and points to root page table
- **TLB**: Cache of recent virtual→physical translations (16 entries, LRU)
- **Page fault**: Exception when accessing unmapped or permission-denied page
- **Page Table Entry (PTE)**: Contains physical page number + permission flags (V/R/W/X/U)

---

## Quick Run (One Command)

```bash
make demo-12
```

This runs `demos/demo_14_mmu.py` which sets up page tables and verifies the MMU configuration.

---

## Expected Output

```
Part 1: Page table layout (root PT, L2 PT, data page, code page)
Part 2: Load into simulator
Part 3: SATP = 0x80000080, MMU enabled
Part 4: Root PT[0] valid, L2 PT[1] maps RW, L2 PT[2] maps RX, L2 PT[3] unmapped
Part 5: TLB size=16, page size=4KB, levels=2
```

---

## Integration Note

The MMU hardware is implemented and configurable, but is **not yet wired into the CPU's load/store path**. This means:
- Page tables can be set up and verified-
- SATP CSR enables/disables the MMU-
- TLB and page fault counters work-
- But actual instruction execution still uses physical addresses directly

Full integration (where every LW/SW goes through `mmu_.translate()`) will be done when CERBERUS OS needs it. This avoids breaking all existing demos (Phases 1-13) that use physical addresses.

---

## What This Demo Proves

- Sv32 page tables correctly stored in physical memory
- SATP CSR enables MMU (MODE=1, PPN set)
- Two-level page table structure verified (root → L2 → physical frame)
- Permission bits (R/W/X/U) correctly encoded in PTEs
- TLB/page fault statistics accessible from Python
- Memory expanded to 1MB (supports page tables + multiple address spaces)
- All previous phases still work (MMU disabled by default)