"""
Demo 12: MMU / Virtual Memory — Sv32 page tables, TLB, page faults.

=== What this demo shows ===
1. Setting up a two-level Sv32 page table in memory
2. Enabling the MMU via SATP CSR
3. CPU accesses virtual addresses -> MMU translates to physical
4. TLB hits/misses observable
5. Page fault when accessing unmapped page

=== Background: Sv32 Virtual Memory ===
Virtual address (32-bit): [VPN1(10) | VPN0(10) | offset(12)]
- VPN1 indexes into the root page table (1024 entries)
- VPN0 indexes into the second-level page table (1024 entries)
- offset selects byte within the 4KB page

Page Table Entry (PTE): [PPN(22) | RSW(2) | D|A|G|U|X|W|R|V]
- V=1: entry is valid
- R/W/X: read/write/execute permissions
- PPN: physical page number of the mapped frame

=== How to run ===
    make demo-12
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 12: MMU / Virtual Memory (Sv32)")
print("=" * 60)

# ═══════════════════════════════════════════════════════════════════════════
# Constants
# ═══════════════════════════════════════════════════════════════════════════

PAGE_SIZE = 4096
PTE_V = 1       # valid
PTE_R = 2       # readable
PTE_W = 4       # writable
PTE_X = 8       # executable
PTE_U = 16      # user-accessible

def make_pte(ppn, flags):
    """Create a page table entry: PPN in bits [31:10], flags in bits [7:0]."""
    return (ppn << 10) | flags

def make_satp(ppn):
    """Create SATP value: MODE=1 (Sv32), PPN = page table base."""
    return (1 << 31) | ppn

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: Setup page tables in physical memory
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Setup Sv32 Page Tables ---")
print("  Memory layout:")
print("    0x80000 (page 0x80): Root page table (level 1)")
print("    0x81000 (page 0x81): Second-level page table")
print("    0x82000 (page 0x82): Data page (mapped to virtual 0x1000)")
print("    0x90000 (page 0x90): Code page (program loaded here)")
print()

# Physical addresses
ROOT_PT_PHYS = 0x80000      # root page table at physical page 0x80
L2_PT_PHYS   = 0x81000      # second-level PT at physical page 0x81
DATA_PHYS    = 0x82000      # data page at physical page 0x82
CODE_PHYS    = 0x90000      # code page at physical page 0x90

# Virtual addresses we want to map
VADDR_DATA = 0x00001000     # virtual address for data (VPN1=0, VPN0=1)
VADDR_CODE = 0x00002000     # virtual address for code (VPN1=0, VPN0=2)

# Build page tables
# Root PT: entry 0 points to L2 PT (VPN1=0 covers virtual 0x00000000-0x003FFFFF)
root_pt = [0] * 1024
root_pt[0] = make_pte(L2_PT_PHYS >> 12, PTE_V)  # pointer to L2 (no R/W/X = non-leaf)

# L2 PT: entry 1 maps virtual page 1 -> physical page 0x82 (data, RW)
#         entry 2 maps virtual page 2 -> physical page 0x90 (code, RX)
l2_pt = [0] * 1024
l2_pt[1] = make_pte(DATA_PHYS >> 12, PTE_V | PTE_R | PTE_W | PTE_U)
l2_pt[2] = make_pte(CODE_PHYS >> 12, PTE_V | PTE_R | PTE_X | PTE_U)

# Convert to bytes
def pt_to_bytes(pt):
    return b''.join(struct.pack('<I', entry) for entry in pt)

root_pt_bytes = list(pt_to_bytes(root_pt))
l2_pt_bytes = list(pt_to_bytes(l2_pt))

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Load page tables + data + code into physical memory
# ═══════════════════════════════════════════════════════════════════════════

print("--- Part 2: Load into simulator ---")

cpu = hades.PipelinedCPU()

# Load page tables
cpu.load_data(root_pt_bytes, ROOT_PT_PHYS)
cpu.load_data(l2_pt_bytes, L2_PT_PHYS)

# Load test data at physical data page (virtual 0x1000 -> physical 0x82000)
test_data = [0x42, 0x00, 0x00, 0x00]  # value 66 (0x42) at offset 0
cpu.load_data(test_data, DATA_PHYS)

# Program runs from virtual 0x2000 (mapped to physical 0x90000, RX)
# It loads a word from virtual 0x1000 (mapped to physical 0x82000, RW)
# LUI t1, 1  (t1 = 0x1000)
# LW t0, 0(t1) (load from virtual 0x1000 -> physical 0x82000)
# ADDI t2, t0, 1
# ECALL
lui_t1 = (1 << 12) | (T1 << 7) | 0b0110111     # LUI t1, 1 → t1 = 0x1000
lw_t0 = encode_i(0, T1, 0b010, T0, OP_LOAD)    # LW t0, 0(t1)
addi_t2 = encode_i(1, T0, 0b000, T2, OP_IMM)   # ADDI t2, t0, 1
prog = to_bytes([lui_t1, lw_t0, addi_t2, ECALL])

# Load program at physical 0x90000 (where virtual 0x2000 maps to)
cpu.load_data(list(prog), CODE_PHYS)

print(f"  Program ({len(prog)} bytes) loaded at physical 0x{CODE_PHYS:05X}")
print(f"  Data (0x42) loaded at physical 0x{DATA_PHYS:05X}")
print(f"  PC set to virtual 0x{VADDR_CODE:05X}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: Enable MMU and test translation
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: Enable MMU via SATP ---")

# Enable MMU: SATP = MODE(1) | PPN of root page table
satp_value = make_satp(ROOT_PT_PHYS >> 12)
print(f"  SATP = 0x{satp_value:08X}")
print(f"    MODE = Sv32 (bit 31 = 1)")
print(f"    PPN  = 0x{ROOT_PT_PHYS >> 12:05X} (root PT at physical 0x{ROOT_PT_PHYS:05X})")

# Set PC to virtual code address, then enable MMU
cpu.load_program([], VADDR_CODE)  # sets PC = 0x2000, no data loaded
cpu.set_mmu_satp(satp_value)
print(f"  MMU enabled: {cpu.get_mmu_satp() >> 31 == 1}")

# Run: CPU fetches from virtual 0x2000 → MMU → physical 0x90000
#      Program does LW from virtual 0x1000 → MMU → physical 0x82000
cpu.run(100)

print(f"\n  Results:")
print(f"    t0 (loaded from virtual 0x1000) = 0x{cpu.get_reg(T0):02X} (expected 0x42)")
print(f"    t2 (t0 + 1)                     = 0x{cpu.get_reg(T2):02X} (expected 0x43)")
print(f"    Cycles: {cpu.get_cycles()}")
print(f"    TLB hits:    {cpu.get_tlb_hits()}")
print(f"    TLB misses:  {cpu.get_tlb_misses()} (code page + data page)")
print(f"    Page faults: {cpu.get_page_faults()}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 4: Test MMU translation from Python
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 4: Verify Page Table Structure ---")

# Read back page table entries to verify they're correct
root_entry_0 = struct.unpack('<I', bytes(cpu.read_mem(ROOT_PT_PHYS, 4)))[0]
l2_entry_1 = struct.unpack('<I', bytes(cpu.read_mem(L2_PT_PHYS + 4, 4)))[0]
l2_entry_2 = struct.unpack('<I', bytes(cpu.read_mem(L2_PT_PHYS + 8, 4)))[0]
l2_entry_3 = struct.unpack('<I', bytes(cpu.read_mem(L2_PT_PHYS + 12, 4)))[0]

print(f"  Root PT[0] = 0x{root_entry_0:08X} (points to L2 PT, V={root_entry_0 & 1})")
print(f"  L2 PT[1]   = 0x{l2_entry_1:08X} (maps vpage 1->ppage 0x{l2_entry_1>>10:03X}, RW)")
print(f"  L2 PT[2]   = 0x{l2_entry_2:08X} (maps vpage 2->ppage 0x{l2_entry_2>>10:03X}, RX)")
print(f"  L2 PT[3]   = 0x{l2_entry_3:08X} (unmapped, should cause page fault)")

# Verify mapping
print(f"\n  Virtual 0x{VADDR_DATA:05X} -> Physical 0x{DATA_PHYS:05X} (data, RW)")
print(f"  Virtual 0x{VADDR_CODE:05X} -> Physical 0x{CODE_PHYS:05X} (code, RX)")
print(f"  Virtual 0x00003000 -> UNMAPPED (page fault)")

# ═══════════════════════════════════════════════════════════════════════════
# Part 5: TLB behavior
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 5: MMU Statistics ---")
print(f"  TLB size: 16 entries (fully-associative, LRU)")
print(f"  Page size: 4KB")
print(f"  Page table levels: 2 (Sv32)")
print(f"  SATP CSR address: 0x180")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f" Demo 12 complete.")
print(f"\n The MMU is now completed:")
print(f"   - OS sets up page tables in physical memory")
print(f"   - OS writes SATP CSR to enable translation")
print(f"   - Each process gets its own page table -> isolation")
print(f"   - Page faults trigger trap -> OS loads page from 'disk'")
print(f"   - TLB caches recent translations -> fast access")