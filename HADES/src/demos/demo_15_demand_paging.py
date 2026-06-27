"""
Demo 15: Demand Paging - page fault trap loads pages from disk.

=== What this demo shows ===
1. MMU enabled with one page intentionally UNMAPPED
2. CPU accesses unmapped virtual page -> page fault exception
3. Trap handler: loads page from disk via DMA, maps it, flushes TLB
4. MRET retries the faulting instruction - succeeds

=== Key insight ===
The trap handler must run with its own memory accessible. We identity-map
the low physical region (virtual = physical) so the handler can access
page tables, scratch RAM, and I/O devices without faulting itself.

=== How to run ===
    make demo-15
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 15: Demand Paging (Page Fault + Disk Load)")
print("=" * 60)

# ===================================================================
# Constants
# ===================================================================

PAGE_SIZE = 4096
PTE_V = 1
PTE_R = 2
PTE_W = 4
PTE_X = 8

def make_pte(ppn, flags):
    return (ppn << 10) | flags

def make_satp(ppn):
    return (1 << 31) | ppn

def write_word(data, addr, val):
    data[addr:addr+4] = val.to_bytes(4, 'little')

# ===================================================================
# Memory layout (all identity-mapped except the demand-paged region)
#
#   Physical / Virtual:
#   0x00000-0x00FFF: scratch (frame pointer at 0x100, result at 0x200)
#   0x01000-0x01FFF: main program (identity-mapped)
#   0x02000-0x02FFF: trap handler (identity-mapped)
#   0x10000-0x10FFF: root page table
#   0x11000-0x11FFF: L2 page table for VPN1=0
#   0x12000-0x12FFF: L2 page table for VPN1=1 (for high virtual addresses)
#   0x30000-0x30FFF: free frame (DMA target)
#
#   Virtual only:
#   0x00400000 (VPN1=1, VPN0=0): UNMAPPED data page (demand paged from disk)
# ===================================================================

ROOT_PT    = 0x10000
L2_PT_LOW  = 0x11000   # L2 for VPN1=0 (identity-mapped low region)
L2_PT_HIGH = 0x12000   # L2 for VPN1=1 (demand-paged region)
CODE_PHYS  = 0x01000
TRAP_PHYS  = 0x02000
FREE_FRAME = 0x30000

# Virtual address that will page fault (VPN1=1, VPN0=0 -> 0x00400000)
VADDR_DEMAND = 0x00400000

DISK_BASE = 0xF0A0

print("\n--- Part 1: Setup Page Tables ---")
print(f"  Identity-mapped: 0x00000-0x3FFFF (code, handler, page tables)")
print(f"  UNMAPPED: virt 0x{VADDR_DEMAND:08X} (will page fault)")

mem = bytearray(0x40000)  # 256KB

# Root page table:
# Entry 0 (VPN1=0): points to L2_PT_LOW (identity-maps low 4MB)
write_word(mem, ROOT_PT + 0*4, make_pte(L2_PT_LOW >> 12, PTE_V))
# Entry 1 (VPN1=1): points to L2_PT_HIGH (demand-paged region)
write_word(mem, ROOT_PT + 1*4, make_pte(L2_PT_HIGH >> 12, PTE_V))

# L2_PT_LOW: identity-map pages 0x00-0x3F (covers 0x00000-0x3FFFF)
# Page 0x0F (0xF000-0xFFFF) is the I/O region - must be mapped for MMU
# translation to succeed; actual I/O accesses are routed by the bus, not RAM.
for i in range(0x40):
    write_word(mem, L2_PT_LOW + i*4,
               make_pte(i, PTE_V | PTE_R | PTE_W | PTE_X))

# L2_PT_HIGH: entry 0 (virt 0x00400000) is UNMAPPED
write_word(mem, L2_PT_HIGH + 0*4, 0)  # not valid -> page fault

# Scratch: free frame pointer at 0x100
write_word(mem, 0x100, FREE_FRAME)

# ===================================================================
# Part 2: Disk image
# ===================================================================

print("\n--- Part 2: Prepare Disk ---")
disk_image = bytearray(PAGE_SIZE)
disk_image[0:4] = (0xDEADBEEF).to_bytes(4, 'little')
disk_image[4:8] = (0xCAFEBABE).to_bytes(4, 'little')
print(f"  Disk sector 0: 0xDEADBEEF (page data for virt 0x{VADDR_DEMAND:08X})")

# ===================================================================
# Part 3: Trap handler at 0x02000 (identity-mapped)
# ===================================================================

print("\n--- Part 3: Build Trap Handler ---")

# Handler:
#   1. Load free frame from scratch[0x100]
#   2. DMA read sector 0 into frame
#   3. Build PTE: (frame >> 12) << 10 | V|R|W = frame >> 2 | 7
#   4. Write PTE to L2_PT_HIGH[0]
#   5. Rewrite SATP to flush TLB
#   6. MRET

L2_HIGH_ENTRY0 = L2_PT_HIGH  # PTE for VPN1=1, VPN0=0

trap_code = [
    # t0 = mem[0x100] = free frame address
    encode_i(0x100, ZERO, 0b000, T0, OP_IMM),
    encode_i(0, T0, 0b010, T0, OP_LOAD),

    # DMA: disk read sector 0 into frame
    encode_u(0x0000F000, T4, OP_LUI),
    encode_i(0x0A0, T4, 0b000, T4, OP_IMM),         # t4 = 0xF0A0
    encode_s(0x04, ZERO, T4, 0b010, OP_STORE),      # SECTOR = 0
    encode_s(0x08, T0, T4, 0b010, OP_STORE),        # BUFFER = frame
    encode_i(1, ZERO, 0b000, T1, OP_IMM),
    encode_s(0x00, T1, T4, 0b010, OP_STORE),        # COMMAND = READ (DMA fires)

    # Build PTE: (t0 >> 2) | 7
    encode_i(2, T0, 0b101, T1, OP_IMM),             # t1 = t0 >> 2 (SRLI)
    encode_i(PTE_V | PTE_R | PTE_W, T1, 0b110, T1, OP_IMM),  # t1 |= 7 (ORI)

    # Write PTE to L2_PT_HIGH + 0
    encode_u(L2_HIGH_ENTRY0 & 0xFFFFF000, T2, OP_LUI),
    encode_i(L2_HIGH_ENTRY0 & 0xFFF, T2, 0b000, T2, OP_IMM),
    encode_s(0, T1, T2, 0b010, OP_STORE),            # mem[L2_PT_HIGH] = PTE

    # Flush TLB: rewrite SATP
    encode_u(make_satp(ROOT_PT >> 12) & 0xFFFFF000, T3, OP_LUI),
    encode_i(make_satp(ROOT_PT >> 12) & 0xFFF, T3, 0b000, T3, OP_IMM),
    encode_i(0x180, T3, 0b001, ZERO, OP_SYSTEM),     # csrrw x0, satp, t3

    # MRET - retry faulting instruction
    encode_i(0x302, ZERO, 0b000, ZERO, OP_SYSTEM),
]

trap_bytes = to_bytes(trap_code)
mem[TRAP_PHYS:TRAP_PHYS+len(trap_bytes)] = trap_bytes
print(f"  Handler: {len(trap_code)} instructions at phys 0x{TRAP_PHYS:05X}")

# ===================================================================
# Part 4: Main program at 0x01000 (identity-mapped)
# ===================================================================

print("\n--- Part 4: Build Main Program ---")

main_code = [
    # Set mtvec = TRAP_PHYS (handler address, identity-mapped)
    encode_u(TRAP_PHYS & 0xFFFFF000, T0, OP_LUI),
    encode_i(TRAP_PHYS & 0xFFF, T0, 0b000, T0, OP_IMM),
    encode_i(0x305, T0, 0b001, ZERO, OP_SYSTEM),     # csrrw x0, mtvec, t0

    # Load from VADDR_DEMAND (0x00400000) - will page fault
    encode_u(VADDR_DEMAND & 0xFFFFF000, A0, OP_LUI),
    encode_i(VADDR_DEMAND & 0xFFF, A0, 0b000, A0, OP_IMM),
    encode_i(0, A0, 0b010, T1, OP_LOAD),             # t1 = mem[0x400000] -> FAULT!

    # After handler returns, t1 has the data. Store to 0x200 for verification.
    encode_i(0x200, ZERO, 0b000, T2, OP_IMM),
    encode_s(0, T1, T2, 0b010, OP_STORE),            # mem[0x200] = t1

    ECALL,
]

main_bytes = to_bytes(main_code)
mem[CODE_PHYS:CODE_PHYS+len(main_bytes)] = main_bytes
print(f"  Main: {len(main_code)} instructions at phys 0x{CODE_PHYS:05X}")

# ===================================================================
# Part 5: Run
# ===================================================================

print("\n--- Part 5: Execute ---")

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)
cpu.set_interrupts_enabled(True)

# Load memory
cpu.load_data(list(mem), 0x0000)

# Load disk
cpu.disk_load_image(list(disk_image))

# Enable MMU
cpu.set_mmu_satp(make_satp(ROOT_PT >> 12))

# Start execution at CODE_PHYS (identity-mapped, so virtual = physical)
cpu.load_program([], CODE_PHYS)

cpu.run(100000)

# Verify
result = int.from_bytes(cpu.read_mem(0x200, 4), 'little')
faults = cpu.get_page_faults()

print(f"\n  Page faults taken: {faults}")
print(f"  Value at 0x200: 0x{result:08X}")
print(f"  Expected:       0xDEADBEEF")
print(f"  CPU halted: {cpu.is_halted()}")
correct = result == 0xDEADBEEF and cpu.is_halted()
print(f"  Result: {'Correct' if correct else 'FAILED'}")

# ===================================================================
print(f"\n{'='*60}")
print(f" Demo 15 complete.")
print(f"\n Demand paging enables CERBERUS OS to implement:")
print(f"   - Virtual memory larger than physical RAM")
print(f"   - Lazy loading (only load pages when first accessed)")
print(f"   - Process isolation (each process has its own address space)")
print(f"   - Copy-on-write fork (share pages until written)")
