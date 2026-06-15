#pragma once
#include <cstdint>
#include <vector>
#include "memory.h"

// CRTP base providing common CPU state and accessor implementations.
// Derived must expose no additional requirements beyond having regs_, pc_, mem_.
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

protected:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;
    Memory mem_;

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }
};
