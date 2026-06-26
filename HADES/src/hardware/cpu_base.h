#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "memory.h"
#include "io_bus.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "vga.h"
#include "block_device.h"
#include "executor.h"

// CRTP base providing common CPU state, I/O devices, and accessor implementations.
// The Executor (threading) is owned by the derived class since its lambdas capture `this`.
template<typename Derived>
class CPUBase {
public:
    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000) {
        mem_.load(base_addr, binary);
        pc_ = base_addr;
        halted_ = false;
    }

    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000) {
        mem_.load(base_addr, data);
    }

    uint32_t get_pc() const { return pc_; }
    uint32_t get_reg(uint32_t idx) const { return (idx < 32) ? regs_[idx] : 0; }
    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const { return mem_.dump(addr, len); }
    bool is_halted() const { return halted_; }

    // Configuration
    void set_cache_enabled(bool enabled) { mem_.set_cache_enabled(enabled); }
    void set_miss_penalty(uint32_t cycles) { mem_.set_miss_penalty(cycles); }
    void set_mem_hierarchy_enabled(bool enabled) { mem_.set_hierarchy_enabled(enabled); }

    // Stats
    uint64_t get_icache_misses() const { return mem_.imem().get_cache_misses(); }
    uint64_t get_dcache_misses() const { return mem_.dmem().get_cache_misses(); }
    uint64_t get_sdram_row_hits() const { return mem_.sdram().get_row_hits(); }
    uint64_t get_sdram_row_misses() const { return mem_.sdram().get_row_misses(); }

    // I/O devices
    void set_io_enabled(bool enabled) { io_enabled_ = enabled; }
    bool get_io_enabled() const { return io_enabled_; }
    void uart_send(const std::vector<uint8_t>& data) { uart_.host_send(data); }
    std::vector<uint8_t> uart_recv() { return uart_.host_recv(); }
    void gpio_set_input(uint32_t value) { gpio_.set_input(value); }
    uint32_t gpio_get_output() const { return gpio_.get_output(); }
    std::vector<uint16_t> vga_get_framebuffer() const { return vga_.get_framebuffer(); }
    std::vector<uint8_t> vga_get_char_buffer() const { return vga_.get_char_buffer(); }
    std::vector<uint8_t> vga_get_color_buffer() const { return vga_.get_color_buffer(); }
    std::string vga_get_char_row(uint32_t row) const { return vga_.get_char_row(row); }

    // MMU / Virtual Memory (delegates to Memory's internal MMU)
    void set_mmu_satp(uint32_t satp) { mem_.mmu().set_satp(satp); }
    uint32_t get_mmu_satp() const { return mem_.mmu().get_satp(); }
    void mmu_flush_tlb() { mem_.mmu().flush_tlb(); }
    uint64_t get_tlb_hits() const { return mem_.mmu().get_tlb_hits(); }
    uint64_t get_tlb_misses() const { return mem_.mmu().get_tlb_misses(); }
    uint64_t get_page_faults() const { return mem_.mmu().get_page_faults(); }

    // Block Storage Device
    void disk_load_image(const std::vector<uint8_t>& data) { disk_.load_image(data); }
    std::vector<uint8_t> disk_save_image() const { return disk_.save_image(); }
    void disk_write_sector(uint32_t sector, const std::vector<uint8_t>& data) { disk_.write_sector(sector, data); }
    std::vector<uint8_t> disk_read_sector(uint32_t sector) { return disk_.read_sector(sector); }
    uint64_t get_disk_reads() const { return disk_.get_reads(); }
    uint64_t get_disk_writes() const { return disk_.get_writes(); }

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

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }

    static uint8_t dma_read(void* ctx, uint32_t addr) {
        return static_cast<CPUBase*>(ctx)->mem_.dmem().read_byte(addr);
    }
    static void dma_write(void* ctx, uint32_t addr, uint8_t val) {
        static_cast<CPUBase*>(ctx)->mem_.dmem().write_byte(addr, val);
    }

    void reset_base() {
        for (int i = 0; i < 32; i++) regs_[i] = 0;
        pc_ = 0x1000;
        halted_ = false;
        io_enabled_ = false;
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
