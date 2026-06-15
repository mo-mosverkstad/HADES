#pragma once
#include <cstdint>
#include <vector>
#include "memory.h"
#include "cache.h"

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

    void set_cache_enabled(bool enabled) { cache_enabled_ = enabled; }
    void set_miss_penalty(uint32_t cycles) { miss_penalty_ = cycles; }
    uint64_t get_icache_misses() const { return icache_.get_misses(); }
    uint64_t get_dcache_misses() const { return dcache_.get_misses(); }

protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;
    Memory mem_;

    Cache icache_;
    Cache dcache_;
    bool cache_enabled_ = false;
    uint32_t miss_penalty_ = 20;

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }
};
