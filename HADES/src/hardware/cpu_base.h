#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "memory.h"
#include "io_bus.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "vga.h"
#include "block_device.h"
#include "executor.h"
#include "pipeline.h"

/**
 * CRTP base providing common CPU state, I/O devices, and accessor implementations.
 * The Executor (threading) is owned by the derived class since its lambdas capture `this`.
 * @tparam Derived The concrete CPU class (CPU or PipelinedCPU).
 */
template<typename Derived>
class CPUBase {
public:
    /** Loads binary into memory at base_addr and sets PC to that address. */
    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000) {
        mem_.load(base_addr, binary);
        pc_ = base_addr;
        halted_ = false;
    }

    /** Loads raw data into memory at base_addr without changing PC. */
    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000) {
        mem_.load(base_addr, data);
    }

    /** Returns the current program counter. */
    uint32_t get_pc() const { return pc_; }

    /** Returns value of register idx (0-31). x0 always returns 0. */
    uint32_t get_reg(uint32_t idx) const { return (idx < 32) ? regs_[idx] : 0; }

    /** Reads len bytes from memory starting at addr. */
    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const { return mem_.dump(addr, len); }

    /** Returns true if the CPU has halted (ECALL executed). */
    bool is_halted() const { return halted_; }

    // Configuration

    /** Enables/disables L1 cache simulation. Disabled by default. */
    void set_cache_enabled(bool enabled) { mem_.set_cache_enabled(enabled); }

    /** Sets cache miss penalty in cycles. */
    void set_miss_penalty(uint32_t cycles) { mem_.set_miss_penalty(cycles); }

    /** Enables/disables SDRAM memory hierarchy simulation. */
    void set_mem_hierarchy_enabled(bool enabled) { mem_.set_hierarchy_enabled(enabled); }

    // Stats

    /** Returns total instruction cache misses. */
    uint64_t get_icache_misses() const { return mem_.imem().get_cache_misses(); }

    /** Returns total data cache misses. */
    uint64_t get_dcache_misses() const { return mem_.dmem().get_cache_misses(); }

    /** Returns SDRAM row buffer hits. */
    uint64_t get_sdram_row_hits() const { return mem_.sdram().get_row_hits(); }

    /** Returns SDRAM row buffer misses. */
    uint64_t get_sdram_row_misses() const { return mem_.sdram().get_row_misses(); }

    // I/O devices

    /** Enables/disables I/O device access (memory-mapped at 0xF000+). */
    void set_io_enabled(bool enabled) { io_enabled_ = enabled; }

    /** Returns whether I/O devices are enabled. */
    bool get_io_enabled() const { return io_enabled_; }

    /** Sends data bytes into the UART RX FIFO (host -> CPU). */
    void uart_send(const std::vector<uint8_t>& data) { uart_.host_send(data); }

    /** Retrieves bytes from the UART TX FIFO (CPU -> host). */
    std::vector<uint8_t> uart_recv() { return uart_.host_recv(); }

    /** Sets GPIO input pins value (host -> CPU). */
    void gpio_set_input(uint32_t value) { gpio_.set_input(value); }

    /** Returns GPIO output pins value (CPU -> host). */
    uint32_t gpio_get_output() const { return gpio_.get_output(); }

    /** Returns the full VGA pixel framebuffer (320x240, RGB565). */
    std::vector<uint16_t> vga_get_framebuffer() const { return vga_.get_framebuffer(); }

    /** Returns the VGA character buffer (80x60 ASCII values). */
    std::vector<uint8_t> vga_get_char_buffer() const { return vga_.get_char_buffer(); }

    /** Returns the VGA color buffer (80x60 color attributes). */
    std::vector<uint8_t> vga_get_color_buffer() const { return vga_.get_color_buffer(); }

    /** Returns a single row of VGA character buffer as a string. */
    std::string vga_get_char_row(uint32_t row) const { return vga_.get_char_row(row); }

    // MMU / Virtual Memory (delegates to Memory's internal MMU)

    /** Sets SATP register to enable/configure MMU page table. */
    void set_mmu_satp(uint32_t satp) { mem_.mmu().set_satp(satp); }

    /** Returns current SATP register value. */
    uint32_t get_mmu_satp() const { return mem_.mmu().get_satp(); }

    /** Invalidates all TLB entries. */
    void mmu_flush_tlb() { mem_.mmu().flush_tlb(); }

    /** Returns total TLB hits. */
    uint64_t get_tlb_hits() const { return mem_.mmu().get_tlb_hits(); }

    /** Returns total TLB misses (page table walks). */
    uint64_t get_tlb_misses() const { return mem_.mmu().get_tlb_misses(); }

    /** Returns total page faults (unmapped/permission violations). */
    uint64_t get_page_faults() const { return mem_.mmu().get_page_faults(); }

    // Block Storage Device

    /** Loads a raw disk image (must be multiple of 512 bytes). */
    void disk_load_image(const std::vector<uint8_t>& data) { disk_.load_image(data); }

    /** Returns the full disk contents as raw bytes. */
    std::vector<uint8_t> disk_save_image() const { return disk_.save_image(); }

    /** Writes a 512-byte sector directly (host-side, bypasses DMA). */
    void disk_write_sector(uint32_t sector, const std::vector<uint8_t>& data) { disk_.write_sector(sector, data); }

    /** Reads a 512-byte sector directly (host-side, bypasses DMA). */
    std::vector<uint8_t> disk_read_sector(uint32_t sector) { return disk_.read_sector(sector); }

    /** Returns total disk read operations count. */
    uint64_t get_disk_reads() const { return disk_.get_reads(); }

    /** Returns total disk write operations count. */
    uint64_t get_disk_writes() const { return disk_.get_writes(); }

    // CSR access (shared by all processor models)

    /** Reads a CSR register. Performance counters are read-only from hardware. */
    uint32_t csr_read(uint32_t addr) const {
        switch (addr) {
            case CSR_MCYCLE:       return (uint32_t)(perf_.mcycle);
            case CSR_MCYCLEH:      return (uint32_t)(perf_.mcycle >> 32);
            case CSR_MINSTRET:     return (uint32_t)(perf_.minstret);
            case CSR_MINSTRETH:    return (uint32_t)(perf_.minstret >> 32);
            case CSR_MHPMCOUNTER3: return (uint32_t)(perf_.stalls_data);
            case CSR_MHPMCOUNTER4: return (uint32_t)(perf_.stalls_branch);
            case CSR_SATP: return mem_.mmu().get_satp();
            default: {
                auto it = csrs_.find(addr);
                return (it != csrs_.end()) ? it->second : 0;
            }
        }
    }

    /** Writes a CSR register. Writing SATP configures the MMU. */
    void csr_write(uint32_t addr, uint32_t value) {
        csrs_[addr] = value;
        if (addr == CSR_SATP) mem_.mmu().set_satp(value);
    }

    // Interrupts

    /** Enables/disables interrupt handling globally. */
    void set_interrupts_enabled(bool enabled) { interrupts_enabled_ = enabled; }

    /** Returns whether interrupts are currently enabled. */
    bool get_interrupts_enabled() const { return interrupts_enabled_; }

    /**
     * Checks for pending device IRQs and takes interrupt if enabled.
     * Saves PC to mepc, sets mcause, disables interrupts, jumps to mtvec.
     */
    void check_interrupts() {
        if (!interrupts_enabled_) return;
        if (!io_enabled_) return;
        if (!io_bus_.any_irq_pending()) return;

        uint32_t mtvec = csr_read(CSR_MTVEC);
        if (mtvec == 0) return;

        csr_write(CSR_MEPC, pc_);
        csr_write(CSR_MCAUSE, 0x80000007);  // interrupt bit | timer (code 7)
        interrupts_enabled_ = false;
        pc_ = mtvec;
        interrupt_taken_ = true;         // signal pipeline to flush
    }

    /**
     * Checks if a memory port faulted, and if so takes the exception.
     * Returns true if a fault was handled (caller should abort the instruction).
     */
    bool handle_fault(MemoryPort& port) {
        if (!port.has_fault()) return false;
        if (!take_exception(port.get_fault_type(), port.get_fault_addr()))
            halted_ = true;
        port.clear_fault();
        return true;
    }

    /**
     * Takes a synchronous exception (page fault). Saves faulting PC to mepc,
     * sets mcause (no interrupt bit), stores faulting address in mtval.
     * Returns true if trap was taken, false if no handler (halts instead).
     */
    bool take_exception(uint32_t cause, uint32_t tval) {
        uint32_t mtvec = csr_read(CSR_MTVEC);
        if (mtvec == 0) return false;  // no handler installed

        csr_write(CSR_MEPC, pc_);      // retry this instruction on MRET
        csr_write(CSR_MCAUSE, cause);  // exception code (bit 31 = 0)
        csr_write(CSR_MTVAL, tval);    // faulting address
        interrupts_enabled_ = false;
        pc_ = mtvec;
        interrupt_taken_ = true;       // signal pipeline to flush
        return true;
    }

protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;

    bool io_enabled_ = false;
    IOBus io_bus_;
    Memory mem_{io_bus_, io_enabled_};
    Timer timer_;
    UART uart_;
    GPIO gpio_;
    VGA vga_;
    BlockDevice disk_;

    // Shared CSR and performance state
    std::unordered_map<uint32_t, uint32_t> csrs_;
    PerfCounters perf_;
    bool interrupts_enabled_ = false;
    bool interrupt_taken_ = false;

    /** Writes value to register rd. x0 writes are ignored. */
    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }

    /** DMA read callback: reads a byte directly from physical memory (bypasses MMU). */
    static uint8_t dma_read(void* ctx, uint32_t addr) {
        return static_cast<CPUBase*>(ctx)->mem_.hierarchy().read_byte(addr);
    }

    /** DMA write callback: writes a byte directly to physical memory (bypasses MMU). */
    static void dma_write(void* ctx, uint32_t addr, uint8_t val) {
        static_cast<CPUBase*>(ctx)->mem_.hierarchy().write_byte(addr, val);
    }

    /** Resets all CPU state, registers, memory, and re-registers I/O devices. */
    void reset_base() {
        for (int i = 0; i < 32; i++) regs_[i] = 0;
        pc_ = 0x1000;
        halted_ = false;
        io_enabled_ = false;
        interrupts_enabled_ = false;
        interrupt_taken_ = false;
        csrs_.clear();
        perf_.reset();
        mem_.reset();
        timer_.reset();
        uart_.reset();
        gpio_.reset();
        vga_.reset();
        io_bus_ = IOBus();
        io_bus_.register_device(0xF000, &timer_);
        io_bus_.register_device(0xF020, &uart_);
        io_bus_.register_device(0xF040, &gpio_);
        io_bus_.register_device(0xF080, &vga_);
        io_bus_.register_device(0xF0A0, &disk_);
        disk_.set_mem_callbacks(this, dma_read, dma_write);
    }
};
