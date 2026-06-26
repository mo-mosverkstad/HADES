"""HADES - Hardware Attack & Defense Experimental Simulator.

This exports:
  - CPU: single-cycle RV32I processor model
  - PipelinedCPU: 3-stage pipelined RV32I processor (IF/ID -> EX -> MEM/WB)
  - MultiCore: dual-core controller with shared memory and I/O
  - PerfCounters: pipeline performance counters (mcycle, minstret, stalls)

Execution modes:
  - run(N) with N > 0: blocking, executes up to N instructions then returns
  - run(0): non-blocking, starts CPU on background thread, returns immediately
  - stop(): pauses background execution, preserves state

I/O devices (memory-mapped at 0xF000+):
  - Timer at 0xF000 (countdown, IRQ)
  - UART at 0xF020 (serial RX/TX FIFOs)
  - GPIO at 0xF040 (parallel I/O, edge IRQ)
  - VGA at 0xF080 (char 80x60 + pixel 320x240)
  - Disk at 0xF0A0 (512-byte sectors, DMA)
"""

from typing import List

class PerfCounters:
    """Pipeline performance counters (read-only fields)."""
    mcycle: int
    minstret: int
    stalls_data: int
    stalls_branch: int

class CPU:
    """Single-cycle RV32I CPU model."""

    def __init__(self) -> None: ...
    def load_program(self, binary: List[int], base_addr: int = 0x1000) -> None:
        """Loads binary into memory at base_addr and sets PC."""
    def load_data(self, data: List[int], base_addr: int = 0x0000) -> None:
        """Loads raw data into memory without changing PC."""
    def run(self, max_instructions: int = 1000000) -> None:
        """Executes up to max_instructions (blocking). 0 = async on background thread."""
    def stop(self) -> None:
        """Stops async execution. State is preserved."""
    def is_running(self) -> bool:
        """Returns True if executing on background thread."""
    def is_halted(self) -> bool:
        """Returns True if CPU halted (ECALL)."""
    def reset(self) -> None:
        """Resets all state to initial values."""
    def get_cycles(self) -> int:
        """Returns total elapsed cycles."""
    def get_pc(self) -> int:
        """Returns current program counter."""
    def get_reg(self, idx: int) -> int:
        """Returns value of register x[idx]."""
    def read_mem(self, addr: int, len: int) -> List[int]:
        """Reads len bytes from memory."""
    def set_cache_enabled(self, enabled: bool) -> None:
        """Enables/disables L1 cache simulation."""
    def set_miss_penalty(self, cycles: int) -> None:
        """Sets cache miss penalty in cycles."""
    def get_icache_misses(self) -> int:
        """Returns instruction cache miss count."""
    def get_dcache_misses(self) -> int:
        """Returns data cache miss count."""
    def set_mem_hierarchy_enabled(self, enabled: bool) -> None:
        """Enables/disables SDRAM hierarchy model."""
    def get_sdram_row_hits(self) -> int:
        """Returns SDRAM row buffer hits."""
    def get_sdram_row_misses(self) -> int:
        """Returns SDRAM row buffer misses."""
    def set_io_enabled(self, enabled: bool) -> None:
        """Enables/disables memory-mapped I/O (0xF000+)."""
    def get_io_enabled(self) -> bool:
        """Returns whether I/O is enabled."""
    def uart_send(self, data: List[int]) -> None:
        """Sends bytes into UART RX FIFO (host -> CPU)."""
    def uart_recv(self) -> List[int]:
        """Retrieves bytes from UART TX FIFO (CPU -> host)."""
    def gpio_set_input(self, value: int) -> None:
        """Sets GPIO input pins (host -> CPU)."""
    def gpio_get_output(self) -> int:
        """Returns GPIO output pins (CPU -> host)."""
    def vga_get_framebuffer(self) -> List[int]:
        """Returns pixel buffer (320x240, RGB565 uint16 values)."""
    def vga_get_char_buffer(self) -> List[int]:
        """Returns character buffer (80x60 ASCII bytes)."""
    def vga_get_color_buffer(self) -> List[int]:
        """Returns color buffer (80x60 color attributes)."""
    def vga_get_char_row(self, row: int) -> str:
        """Returns one row of the character buffer as a string."""
    def set_mmu_satp(self, satp: int) -> None:
        """Sets SATP CSR to enable/configure MMU."""
    def get_mmu_satp(self) -> int:
        """Returns current SATP value."""
    def mmu_flush_tlb(self) -> None:
        """Invalidates all TLB entries."""
    def get_tlb_hits(self) -> int:
        """Returns TLB hit count."""
    def get_tlb_misses(self) -> int:
        """Returns TLB miss count (page walks)."""
    def get_page_faults(self) -> int:
        """Returns page fault count."""
    def disk_load_image(self, data: List[int]) -> None:
        """Loads raw disk image (multiple of 512 bytes)."""
    def disk_save_image(self) -> List[int]:
        """Returns full disk contents as bytes."""
    def disk_write_sector(self, sector: int, data: List[int]) -> None:
        """Writes 512-byte sector directly (host-side)."""
    def disk_read_sector(self, sector: int) -> List[int]:
        """Reads 512-byte sector directly (host-side)."""
    def get_disk_reads(self) -> int:
        """Returns total disk read count."""
    def get_disk_writes(self) -> int:
        """Returns total disk write count."""
    def set_interrupts_enabled(self, enabled: bool) -> None:
        """Enables/disables interrupt handling."""
    def get_interrupts_enabled(self) -> bool:
        """Returns whether interrupts are enabled."""

class PipelinedCPU:
    """3-stage pipelined RV32I CPU model (IF/ID -> EX -> MEM/WB)."""

    def __init__(self) -> None: ...
    def load_program(self, binary: List[int], base_addr: int = 0x1000) -> None:
        """Loads binary into memory at base_addr and sets PC."""
    def load_data(self, data: List[int], base_addr: int = 0x0000) -> None:
        """Loads raw data into memory without changing PC."""
    def run(self, max_instructions: int = 1000000) -> None:
        """Executes up to max_instructions retired (blocking). 0 = async."""
    def stop(self) -> None:
        """Stops async execution. State is preserved."""
    def is_running(self) -> bool:
        """Returns True if executing on background thread."""
    def is_halted(self) -> bool:
        """Returns True if halted AND pipeline fully drained."""
    def reset(self) -> None:
        """Resets all CPU and pipeline state."""
    def get_cycles(self) -> int:
        """Returns total clock cycles (includes stalls)."""
    def get_instret(self) -> int:
        """Returns total retired instructions."""
    def get_pc(self) -> int:
        """Returns current program counter."""
    def get_reg(self, idx: int) -> int:
        """Returns value of register x[idx]."""
    def read_mem(self, addr: int, len: int) -> List[int]:
        """Reads len bytes from memory."""
    def get_perf_counters(self) -> PerfCounters:
        """Returns all performance counters."""
    def set_cache_enabled(self, enabled: bool) -> None:
        """Enables/disables L1 cache simulation."""
    def set_miss_penalty(self, cycles: int) -> None:
        """Sets cache miss penalty in cycles."""
    def get_icache_misses(self) -> int:
        """Returns instruction cache miss count."""
    def get_dcache_misses(self) -> int:
        """Returns data cache miss count."""
    def set_mem_hierarchy_enabled(self, enabled: bool) -> None:
        """Enables/disables SDRAM hierarchy model."""
    def get_sdram_row_hits(self) -> int:
        """Returns SDRAM row buffer hits."""
    def get_sdram_row_misses(self) -> int:
        """Returns SDRAM row buffer misses."""
    def set_io_enabled(self, enabled: bool) -> None:
        """Enables/disables memory-mapped I/O (0xF000+)."""
    def get_io_enabled(self) -> bool:
        """Returns whether I/O is enabled."""
    def uart_send(self, data: List[int]) -> None:
        """Sends bytes into UART RX FIFO (host -> CPU)."""
    def uart_recv(self) -> List[int]:
        """Retrieves bytes from UART TX FIFO (CPU -> host)."""
    def gpio_set_input(self, value: int) -> None:
        """Sets GPIO input pins (host -> CPU)."""
    def gpio_get_output(self) -> int:
        """Returns GPIO output pins (CPU -> host)."""
    def vga_get_framebuffer(self) -> List[int]:
        """Returns pixel buffer (320x240, RGB565 uint16 values)."""
    def vga_get_char_buffer(self) -> List[int]:
        """Returns character buffer (80x60 ASCII bytes)."""
    def vga_get_color_buffer(self) -> List[int]:
        """Returns color buffer (80x60 color attributes)."""
    def vga_get_char_row(self, row: int) -> str:
        """Returns one row of the character buffer as a string."""
    def set_mmu_satp(self, satp: int) -> None:
        """Sets SATP CSR to enable/configure MMU."""
    def get_mmu_satp(self) -> int:
        """Returns current SATP value."""
    def mmu_flush_tlb(self) -> None:
        """Invalidates all TLB entries."""
    def get_tlb_hits(self) -> int:
        """Returns TLB hit count."""
    def get_tlb_misses(self) -> int:
        """Returns TLB miss count (page walks)."""
    def get_page_faults(self) -> int:
        """Returns page fault count."""
    def disk_load_image(self, data: List[int]) -> None:
        """Loads raw disk image (multiple of 512 bytes)."""
    def disk_save_image(self) -> List[int]:
        """Returns full disk contents as bytes."""
    def disk_write_sector(self, sector: int, data: List[int]) -> None:
        """Writes 512-byte sector directly (host-side)."""
    def disk_read_sector(self, sector: int) -> List[int]:
        """Reads 512-byte sector directly (host-side)."""
    def get_disk_reads(self) -> int:
        """Returns total disk read count."""
    def get_disk_writes(self) -> int:
        """Returns total disk write count."""
    def set_interrupts_enabled(self, enabled: bool) -> None:
        """Enables/disables interrupt handling."""
    def get_interrupts_enabled(self) -> bool:
        """Returns whether interrupts are enabled."""

class MultiCore:
    """Dual-core CPU controller with shared memory and I/O."""

    def __init__(self) -> None: ...
    def reset(self) -> None:
        """Resets both cores and all shared state."""
    def load_program(self, core_id: int, binary: List[int], base_addr: int) -> None:
        """Loads program for a specific core."""
    def load_data(self, data: List[int], base_addr: int = 0x0000) -> None:
        """Loads shared data into memory."""
    def run(self, max_cycles: int = 100000) -> None:
        """Runs both cores in round-robin until both halt or max_cycles."""
    def get_reg(self, core_id: int, idx: int) -> int:
        """Returns register value for a specific core."""
    def get_cycles(self, core_id: int) -> int:
        """Returns cycle count for a specific core."""
    def get_instret(self, core_id: int) -> int:
        """Returns retired instruction count for a specific core."""
    def get_global_cycles(self) -> int:
        """Returns total system cycles."""
    def is_halted(self, core_id: int) -> bool:
        """Returns whether a specific core has halted."""
    def read_mem(self, addr: int, len: int) -> List[int]:
        """Reads from shared memory."""
    def get_mutex_contentions(self) -> int:
        """Returns number of failed mutex lock attempts."""
    def get_mutex_locked(self) -> bool:
        """Returns whether the hardware mutex is currently locked."""
    def get_mutex_owner(self) -> int:
        """Returns the current mutex owner ID."""
    def uart_send(self, data: List[int]) -> None:
        """Sends bytes into shared UART RX FIFO."""
    def uart_recv(self) -> List[int]:
        """Retrieves bytes from shared UART TX FIFO."""
    def gpio_set_input(self, value: int) -> None:
        """Sets shared GPIO input pins."""
    def gpio_get_output(self) -> int:
        """Returns shared GPIO output pins."""
