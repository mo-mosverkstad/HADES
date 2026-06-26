"""
Demo 13: Block Storage Device — sector-based disk for file system support.

=== What this demo shows ===
1. Loading a disk image into the simulator
2. Reading/writing sectors from Python (host-side)
3. CPU accessing disk via memory-mapped I/O registers
4. Disk latency simulation (busy -> done transition)

=== Background: Block Devices ===
A block device (disk/SSD/SD card) stores data in fixed-size sectors (512 bytes).
The OS reads/writes entire sectors, not individual bytes.

HADES disk I/O registers (at 0xF0A0):
  0x00 COMMAND:   0=NOP, 1=READ, 2=WRITE
  0x04 SECTOR:    sector number to access
  0x08 BUFFER:    RAM address for DMA transfer
  0x0C STATUS:    0=idle, 1=busy, 2=done, 3=error
  0x10 DISK_SIZE: total sectors (read-only)
  0x14 LATENCY:   cycles per operation

=== How to run ===
    make demo-13
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hades
from riscvtools import *

print("=" * 60)
print("DEMO 13: Block Storage Device (Disk)")
print("=" * 60)

DISK_BASE = 0xF0A0

# ═══════════════════════════════════════════════════════════════════════════
# Part 1: Python-side disk access (host API)
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 1: Host-side Disk Access ---")
print("  Write data to sector 0, read it back\n")

cpu = hades.PipelinedCPU()
cpu.set_io_enabled(True)

# Write "Hello, Disk!" to sector 0
data = list(b'Hello, Disk!' + b'\x00' * (512 - 12))
cpu.disk_write_sector(0, data)

# Read it back
readback = cpu.disk_read_sector(0)
text = bytes(readback[:12]).decode('ascii')
print(f"  Written:  'Hello, Disk!'")
print(f"  Readback: '{text}'")
print(f"  Match: {'Correct' if text == 'Hello, Disk!' else 'Incorrect'}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 2: Disk image load/save
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 2: Disk Image Load/Save ---")

# Create a 4-sector disk image
image = bytearray(4 * 512)
image[0:5] = b'BOOT\x00'           # sector 0: boot marker
image[512:516] = b'DATA'            # sector 1: data marker
image[1024:1028] = b'MORE'          # sector 2
image[1536:1540] = b'LAST'          # sector 3

cpu.disk_load_image(list(image))

# Verify
s0 = bytes(cpu.disk_read_sector(0)[:5])
s1 = bytes(cpu.disk_read_sector(1)[:4])
s2 = bytes(cpu.disk_read_sector(2)[:4])
s3 = bytes(cpu.disk_read_sector(3)[:4])
print(f"  Loaded 4-sector image (2048 bytes)")
print(f"  Sector 0: {s0}")
print(f"  Sector 1: {s1}")
print(f"  Sector 2: {s2}")
print(f"  Sector 3: {s3}")

# Save image back
saved = cpu.disk_save_image()
print(f"  Saved image: {len(saved)} bytes")
print(f"  Round-trip match: {'Correct' if saved == list(image) else 'Incorrect'}")

# ═══════════════════════════════════════════════════════════════════════════
# Part 3: CPU accesses disk via I/O registers
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 3: CPU Disk Access via I/O ---")
print(f"  Disk registers at 0x{DISK_BASE:04X}")
print(f"  CPU reads DISK_SIZE and STATUS registers\n")

# Program: read disk size and status
# DISK_SIZE at offset 0x10, STATUS at offset 0x0C
prog = to_bytes([
    encode_u(0x0000F000, T4, OP_LUI),
    encode_i(0x0A0, T4, 0b000, T4, OP_IMM),    # t4 = 0xF0A0 (disk base)
    # Read DISK_SIZE (offset 0x10)
    encode_i(0x10, T4, 0b010, T0, OP_LOAD),     # lw t0, 0x10(t4) -> disk size
    # Read STATUS (offset 0x0C)
    encode_i(0x0C, T4, 0b010, T1, OP_LOAD),     # lw t1, 0x0C(t4) -> status
    # Read LATENCY (offset 0x14)
    encode_i(0x14, T4, 0b010, T2, OP_LOAD),     # lw t2, 0x14(t4) -> latency
    ECALL,
])

cpu2 = hades.PipelinedCPU()
cpu2.set_io_enabled(True)

cpu2.disk_load_image(list(image))  # 4 sectors
cpu2.load_program(list(prog))
cpu2.run()

disk_size = cpu2.get_reg(5)   # t0
status = cpu2.get_reg(6)      # t1
latency = cpu2.get_reg(7)     # t2
print(f"  DISK_SIZE (sectors): {disk_size}")
print(f"  STATUS: {status} (0=idle)")
print(f"  LATENCY: {latency} cycles per operation")

# ═══════════════════════════════════════════════════════════════════════════
# Part 4: Disk statistics
# ═══════════════════════════════════════════════════════════════════════════

print("\n--- Part 4: Disk Statistics ---")

cpu3 = hades.PipelinedCPU()
cpu3.set_io_enabled(True)

cpu3.disk_load_image(list(image))

# Do some operations
cpu3.disk_read_sector(0)
cpu3.disk_read_sector(1)
cpu3.disk_write_sector(2, list(b'MODIFIED' + b'\x00' * 504))
cpu3.disk_read_sector(2)

print(f"  Operations: 3 reads + 1 write")
print(f"  Disk reads:  {cpu3.get_disk_reads()}")
print(f"  Disk writes: {cpu3.get_disk_writes()}")

# Verify modification persisted
modified = bytes(cpu3.disk_read_sector(2)[:8])
print(f"  Sector 2 after write: {modified}")

# ═══════════════════════════════════════════════════════════════════════════
print(f"\n{'='*60}")
print(f" Demo 13 complete.")
print(f"\n The block device enables CERBERUS OS to implement:")
print(f"   - File system (FAT-like or simple indexed)")
print(f"   - Demand paging (swap pages to/from disk)")
print(f"   - Persistent storage (data survives reboot)")
print(f"   - Boot loader (read kernel from disk sector 0)")