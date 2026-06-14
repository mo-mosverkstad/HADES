#pragma once
#include <cstdint>
#include <vector>
#include "memory.h"

class CPU {
public:
    CPU();

    // Program/data loading
    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000);
    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000);

    // Execution
    void run(uint32_t max_instructions = 1000000);
    void reset();

    // Observation
    uint64_t get_cycles() const;
    uint32_t get_pc() const;
    uint32_t get_reg(uint32_t idx) const;
    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const;


private:
    uint32_t regs_[32];
    uint32_t pc_;
    uint64_t cycles_;
    bool halted_;

    Memory mem_;

    void write_reg(uint32_t rd, uint32_t value);
    void execute(uint32_t instr);

    // Decode helpers
    static uint32_t opcode(uint32_t instr) { return instr & 0x7F; }
    static uint32_t rd(uint32_t instr) { return (instr >> 7) & 0x1F; }
    static uint32_t funct3(uint32_t instr) { return (instr >> 12) & 0x7; }
    static uint32_t rs1(uint32_t instr) { return (instr >> 15) & 0x1F; }
    static uint32_t rs2(uint32_t instr) { return (instr >> 20) & 0x1F; }
    static uint32_t funct7(uint32_t instr) { return (instr >> 25) & 0x7F; }

    static int32_t imm_i(uint32_t instr);
    static int32_t imm_s(uint32_t instr);
    static int32_t imm_b(uint32_t instr);
    static int32_t imm_u(uint32_t instr);
    static int32_t imm_j(uint32_t instr);
};