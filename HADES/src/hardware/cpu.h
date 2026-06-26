#pragma once
#include <cstdint>
#include <vector>
#include "cpu_base.h"

// Single-cycle CPU model.
class CPU : public CPUBase<CPU> {
public:
    CPU();

    // Execution
    void run(uint32_t max_instructions = 1000000);
    void stop() { exec_.stop(); }
    bool is_running() const { return exec_.is_running(); }
    void reset();

    // State access
    uint64_t get_cycles() const { return perf_.mcycle; }

private:
    Executor exec_{[this]{ step(); }, [this]{ return halted_; }};

    void step();
    void execute(uint32_t instr);
};
