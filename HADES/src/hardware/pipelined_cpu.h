#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "memory.h"
#include "pipeline.h"

class PipelinedCPU {
public:
    PipelinedCPU();

    // Program/data loading
    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000);
    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000);

    // Execution
    void run(uint32_t max_instructions = 1000000);
    void reset();

    // Observation
    uint64_t get_cycles() const;
    uint64_t get_instret() const;
    uint32_t get_pc() const;
    uint32_t get_reg(uint32_t idx) const;
    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const;

    // Performance counters (Phase 4)
    PerfCounters get_perf_counters() const;

private:
    // Register file
    uint32_t regs_[32];
    uint32_t pc_;
    bool halted_;

    // Pipeline stages
    StageIFID ifid_;
    StageEX ex_;
    StageMEMWB memwb_;

    // Performance
    PerfCounters perf_;

    // CSR registers
    std::unordered_map<uint32_t, uint32_t> csrs_;

    // Subsystems
    Memory mem_;

    // Pipeline operations
    void pipeline_cycle();
    void stage_writeback();
    void stage_memory();
    void stage_execute();
    void stage_fetch_decode();

    // Forwarding: resolve register value considering pipeline bypasses
    uint32_t forward_reg(uint32_t reg_idx) const;
    bool detect_load_use_hazard() const;

    // Single-cycle fallback (backward compat)
    void execute_single_cycle(uint32_t instr);

    // Write register with leakage
    void write_reg(uint32_t rd, uint32_t value);

    // CSR access
    uint32_t csr_read(uint32_t addr) const;
    void csr_write(uint32_t addr, uint32_t value);

    // ALU + decode helpers
    struct DecodeResult {
        uint32_t opcode, rd, funct3, rs1, rs2, funct7;
        int32_t imm_i, imm_s, imm_b, imm_u, imm_j;
    };
    static DecodeResult decode(uint32_t instr);

    // Execute ALU operation, returns result and metadata
    struct ExecResult {
        uint32_t result;
        bool writes_rd;
        bool is_load;
        bool is_store;
        bool is_branch;
        bool branch_taken;
        uint32_t branch_target;
        uint32_t store_value;
    };
    ExecResult execute_alu(const DecodeResult& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc);
};