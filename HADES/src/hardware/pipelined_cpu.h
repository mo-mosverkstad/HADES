#pragma once
#include <cstdint>
#include <vector>
#include "cpu_base.h"
#include "pipeline.h"

struct Decoded; // forward declare

/** Result of ALU execution in the EX stage. */
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

/**
 * 3-stage pipelined CPU model (IF/ID -> EX -> MEM/WB).
 * Features data forwarding, load-use stall detection, and branch penalty.
 * Supports full RV32I + CSR + interrupts.
 */
class PipelinedCPU : public CPUBase<PipelinedCPU> {
public:
    PipelinedCPU();

    /**
     * Executes up to max_instructions retired then returns (blocking).
     * If max_instructions == 0, starts async execution on background thread (non-blocking).
     */
    void run(uint32_t max_instructions = 1000000);

    /** Stops async execution. CPU state is preserved. */
    void stop() { exec_.stop(); }

    /** Returns true if the CPU is actively executing on the background thread. */
    bool is_running() const { return exec_.is_running(); }

    /** Resets all CPU and pipeline state to initial values. */
    void reset();

    // State access

    /** Returns total elapsed clock cycles (includes stalls). */
    uint64_t get_cycles() const { return perf_.mcycle; }

    /** Returns total retired instructions. */
    uint64_t get_instret() const { return perf_.minstret; }

    /** Returns all performance counters (cycles, instret, stalls). */
    PerfCounters get_perf_counters() const { return perf_; }

    /** Returns true only when halted AND pipeline is fully drained. */
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
