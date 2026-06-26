#pragma once
#include <cstdint>
#include <vector>
#include "cpu_base.h"

/**
 * Single-cycle CPU model.
 * Executes one instruction per call to step(). No pipeline hazards.
 * Supports full RV32I + CSR + interrupts.
 */
class CPU : public CPUBase<CPU> {
public:
    CPU();

    /**
     * Executes up to max_instructions then returns (blocking).
     * If max_instructions == 0, starts async execution on background thread (non-blocking).
     */
    void run(uint32_t max_instructions = 1000000);

    /** Stops async execution. CPU state is preserved. */
    void stop() { exec_.stop(); }

    /** Returns true if the CPU is actively executing on the background thread. */
    bool is_running() const { return exec_.is_running(); }

    /** Resets all CPU state to initial values. */
    void reset();

    // State access

    /** Returns total elapsed cycles. */
    uint64_t get_cycles() const { return perf_.mcycle; }

private:
    Executor exec_{[this]{ step(); }, [this]{ return halted_; }};

    void step();
    void execute(uint32_t instr);
};
