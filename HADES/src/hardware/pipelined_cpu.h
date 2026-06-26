#pragma once
#include <cstdint>
#include <vector>
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

// Pipelined CPU model.
class PipelinedCPU : public CPUBase<PipelinedCPU> {
public:
    PipelinedCPU();

    // Execution
    void run(uint32_t max_instructions = 1000000);
    void stop() { exec_.stop(); }
    bool is_running() const { return exec_.is_running(); }
    void reset();

    // State access
    uint64_t get_cycles() const { return perf_.mcycle; }
    uint64_t get_instret() const { return perf_.minstret; }
    PerfCounters get_perf_counters() const { return perf_; }
    bool is_halted() const { return halted_ && !memwb_.valid; }

private:
    // Pipeline stages
    StageIFID ifid_{};
    StageEX ex_{};
    StageMEMWB memwb_{};

    Executor exec_{[this]{ step(); }, [this]{ return is_halted(); }};

    void step();  // one pipeline cycle
    void pipeline_cycle();
    void stage_writeback();
    void stage_memory();
    void stage_execute();
    void stage_fetch_decode();
    uint32_t forward_reg(uint32_t reg_idx) const;
    bool detect_load_use_hazard() const;
    static ExecResult execute_alu(const Decoded& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc);
};
