#pragma once
#include <cstdint>
#include <vector>
#include "memory.h"

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
    void uart_send(const std::vector<uint8_t>& data);
    std::vector<uint8_t> uart_recv();
    void gpio_set_input(uint32_t value);
    uint32_t gpio_get_output() const;

protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;

    Memory mem_;
    
    // I/O devices (Phase 7)
    IOBus io_bus_;
    Timer timer_;
    UART uart_;
    GPIO gpio_;
    bool io_enabled_;

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }
};
