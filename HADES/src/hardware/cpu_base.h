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

// CRTP base providing common CPU state and accessor implementations.
template<typename Derived>
class CPUBase {
public:
    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000) {
        mem_.load(base_addr, binary);
        pc_ = base_addr;
    }

    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000) {
        mem_.load(base_addr, data);
    }

    uint32_t get_pc() const { return pc_; }

    uint32_t get_reg(uint32_t idx) const {
        return (idx < 32) ? regs_[idx] : 0;
    }

    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const {
        return mem_.dump(addr, len);
    }

    // Configuration
    void set_cache_enabled(bool enabled) { mem_.set_cache_enabled(enabled); }
    void set_miss_penalty(uint32_t cycles) { mem_.set_miss_penalty(cycles); }
    void set_mem_hierarchy_enabled(bool enabled) { mem_.set_hierarchy_enabled(enabled); }

    // Stats
    uint64_t get_icache_misses() const { return mem_.imem().get_cache_misses(); }
    uint64_t get_dcache_misses() const { return mem_.dmem().get_cache_misses(); }
    uint64_t get_sdram_row_hits() const { return mem_.sdram().get_row_hits(); }
    uint64_t get_sdram_row_misses() const { return mem_.sdram().get_row_misses(); }

    // I/O devices (Phase 7)
    void set_io_enabled(bool enabled) { io_enabled_ = enabled; }
    bool get_io_enabled() const { return io_enabled_; }
    void uart_send(const std::vector<uint8_t>& data){ uart_.host_send(data); };
    std::vector<uint8_t> uart_recv() { return uart_.host_recv(); };
    void gpio_set_input(uint32_t value) { gpio_.set_input(value); };
    uint32_t gpio_get_output() const { return gpio_.get_output(); };
    
    // VGA
    std::vector<uint16_t> vga_get_framebuffer() const { return vga_.get_framebuffer(); }
    std::vector<uint8_t> vga_get_char_buffer() const { return vga_.get_char_buffer(); }
    std::string vga_get_char_row(uint32_t row) const { return vga_.get_char_row(row); }

protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;

    // I/O devices (Phase 7) — io_enabled_ and io_bus_ declared before mem_
    bool io_enabled_ = false;
    IOBus io_bus_;
    Memory mem_{io_bus_, io_enabled_};
    Timer timer_;
    UART uart_;
    GPIO gpio_;
    VGA vga_;

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }
};
