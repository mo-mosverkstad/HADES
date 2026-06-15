#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "cpu_base.h"
#include "pipeline.h"

struct Decoded; // forward declare

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

class PipelinedCPU : public CPUBase<PipelinedCPU> {
public:
    PipelinedCPU();

    void run(uint32_t max_instructions = 1000000);
    void reset();

    uint64_t get_cycles() const;
    uint64_t get_instret() const;

    // Performance counters
    PerfCounters get_perf_counters() const;

private:
    // Pipeline stages
    StageIFID ifid_;
    StageEX ex_;
    StageMEMWB memwb_;

    // Performance
    PerfCounters perf_;

    // CSR registers
    std::unordered_map<uint32_t, uint32_t> csrs_;

    // Pipeline operations
    void pipeline_cycle();
    void stage_writeback();
    void stage_memory();
    void stage_execute();
    void stage_fetch_decode();

    uint32_t forward_reg(uint32_t reg_idx) const;
    bool detect_load_use_hazard() const;

    static ExecResult execute_alu(const Decoded& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc);

    // CSR access
    uint32_t csr_read(uint32_t addr) const;
    void csr_write(uint32_t addr, uint32_t value);
};
